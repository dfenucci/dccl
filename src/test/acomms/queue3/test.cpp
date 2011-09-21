// copyright 2011 t. schneider tes@mit.edu
// 
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
//
// This software is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU General Public License for more details.
//
// You should have received a copy of the GNU General Public License
// along with this software.  If not, see <http://www.gnu.org/licenses/>.

#include "test.pb.h"
#include "goby/acomms/queue.h"
#include "goby/acomms/connect.h"
#include "goby/util/binary.h"
#include "goby/util/logger.h"
#include "goby/acomms/dccl/dccl_field_codec_default.h"
// tests basic DCCL queuing with non-BROADCAST destination

using goby::acomms::operator<<;

int receive_count = 0;
GobyMessage msg_in1;
GobyMessage msg_in2;
goby::acomms::protobuf::QueueManagerConfig cfg;
const int MY_MODEM_ID = 1;
const int UNICORN_MODEM_ID = 3;
boost::posix_time::ptime current_time = boost::posix_time::second_clock::universal_time();

int goby_message_qsize = 0;

bool handle_ack_called = false;


void handle_receive(const google::protobuf::Message &msg);

void qsize(goby::acomms::protobuf::QueueSize size);
void handle_ack(const goby::acomms::protobuf::ModemTransmission& ack_msg,
                const google::protobuf::Message& orig_msg);


int main(int argc, char* argv[])
{    
    goby::glog.add_stream(goby::util::logger::DEBUG3, &std::cout);
    goby::glog.set_name(argv[0]);
    
    goby::acomms::QueueManager q_manager;
    cfg.set_modem_id(MY_MODEM_ID);
    q_manager.set_cfg(cfg);
    
    goby::acomms::DCCLModemIdConverterCodec::add("unicorn", UNICORN_MODEM_ID);
    goby::acomms::DCCLModemIdConverterCodec::add("topside", MY_MODEM_ID);
    
    goby::acomms::connect(&q_manager.signal_receive, &handle_receive);
    goby::acomms::connect(&q_manager.signal_queue_size_change, &qsize);
    goby::acomms::connect(&q_manager.signal_ack, &handle_ack);

    msg_in1.set_telegram("hello uni!");
    msg_in1.mutable_header()->set_time(
        goby::util::as<std::string>(current_time));
    msg_in1.mutable_header()->set_source_platform("topside");
    msg_in1.mutable_header()->set_dest_platform("unicorn");
    msg_in1.mutable_header()->set_dest_type(Header::PUBLISH_OTHER);
    
    msg_in2.set_telegram("hello all!");
    msg_in2.mutable_header()->set_time(
        goby::util::as<std::string>(current_time));
    
    msg_in2.mutable_header()->set_source_platform("topside");
    msg_in2.mutable_header()->set_dest_type(Header::PUBLISH_ALL);

    std::cout << "Pushed: " << msg_in1 << std::endl;
    q_manager.push_message(msg_in1);
    assert(goby_message_qsize == 1);
    
    std::cout << "Pushed: " << msg_in2 << std::endl;
    q_manager.push_message(msg_in2);
    assert(goby_message_qsize == 2);

    goby::acomms::protobuf::ModemTransmission transmit_msg;
    transmit_msg.set_max_frame_bytes(256);
    transmit_msg.set_dest(goby::acomms::QUERY_DESTINATION_ID);
    
    q_manager.handle_modem_data_request(&transmit_msg);

    // one ack, one not
    assert(goby_message_qsize == 1);

    std::cout << "requesting data, got: " << transmit_msg << std::endl;
    std::cout << "\tdata as hex: " << goby::util::hex_encode(transmit_msg.frame(0)) << std::endl;


    
    std::list<const google::protobuf::Message*> msgs;
    msgs.push_back(&msg_in1);
    msgs.push_back(&msg_in2);

    assert(transmit_msg.frame(0) == goby::acomms::DCCLCodec::get()->encode_repeated(msgs));
    assert(transmit_msg.src() == MY_MODEM_ID);
    assert(transmit_msg.dest() == UNICORN_MODEM_ID);
    assert(transmit_msg.ack_requested() == true);    

    // feed back the modem layer - the unicorn message will be rejected
    q_manager.handle_modem_receive(transmit_msg);
    assert(receive_count == 1);

    // fake an ack from unicorn
    goby::acomms::protobuf::ModemTransmission ack;
    ack.set_src(UNICORN_MODEM_ID);
    ack.set_dest(MY_MODEM_ID);
    ack.add_acked_frame(0);
    ack.set_type(goby::acomms::protobuf::ModemTransmission::ACK);
    q_manager.handle_modem_receive(ack);

    assert(goby_message_qsize == 0);
    assert(handle_ack_called);
    
    // pretend we're now Unicorn
    cfg.set_modem_id(UNICORN_MODEM_ID);
    q_manager.set_cfg(cfg);
    
    // feed back the modem layer
    q_manager.handle_modem_receive(transmit_msg);
    
    assert(receive_count == 3);

    
    std::cout << "all tests passed" << std::endl;    
}

void handle_receive(const google::protobuf::Message& msg)
{
    std::cout << "Received: " << msg << std::endl;

    GobyMessage typed_msg;
    typed_msg.CopyFrom(msg);    
    
    assert(typed_msg.header().time() == goby::util::as<std::string>(current_time));

    
    if(!typed_msg.header().has_dest_platform())
        assert(typed_msg.SerializeAsString() == msg_in2.SerializeAsString());
    else if(typed_msg.header().dest_platform() == "unicorn")
        assert(typed_msg.SerializeAsString() == msg_in1.SerializeAsString());

    ++receive_count;
}

void qsize(goby::acomms::protobuf::QueueSize size)
{
    goby_message_qsize = size.size();
}


void handle_ack(const goby::acomms::protobuf::ModemTransmission& ack_msg,
                const google::protobuf::Message& orig_msg)
{
    std::cout << "got an ack: " << ack_msg << "\n" 
              << "of original: " << orig_msg << std::endl;
    assert(orig_msg.SerializeAsString() == msg_in1.SerializeAsString());
    handle_ack_called = true;
}
