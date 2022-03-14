// Copyright 2009-2017 Toby Schneider (http://gobysoft.org/index.wt/people/toby)
//                     GobySoft, LLC (for 2013-)
//                     Massachusetts Institute of Technology (for 2007-2014)
//                     Community contributors (see AUTHORS file)
//
//
// This file is part of the Dynamic Compact Control Language Library
// ("DCCL").
//
// DCCL is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 2.1 of the License, or
// (at your option) any later version.
//
// DCCL is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
//
// You should have received a copy of the GNU Lesser General Public License
// along with DCCL.  If not, see <http://www.gnu.org/licenses/>.
// tests all protobuf types with _default codecs, repeat and non repeat

#include <fstream>

#include <google/protobuf/descriptor.pb.h>

#include "dccl/binary.h"
#include "dccl/codec.h"
#include "test.pb.h"
using namespace dccl::test;

dccl::Codec codec;
TestMsg msg_in;

void decode_check(const std::string& encoded);

int main(int argc, char* argv[])
{
    dccl::dlog.connect(dccl::logger::ALL, &std::cerr);

    msg_in.set_state(TestMsg::STATE_1);
    msg_in.set_a(40);
    msg_in.set_b(50);
    msg_in.set_c(60);
    msg_in.set_c_center(50);
    msg_in.add_d(50);
    msg_in.add_d(100);
    msg_in.add_d(150);
    msg_in.add_d(200);
    msg_in.add_d(250);
    msg_in.add_d(300);
    // {
    //     auto c = msg_in.add_child();
    //     c->set_include_i(TestMsg::Child::YES);
    //     c->set_i(1);
    // }
    {
        auto c = msg_in.add_child();
        c->set_include_i(TestMsg::Child::NO);
        c->set_i(25);
        c->set_i2(25);
    }
    {
        auto c = msg_in.add_child();
        c->set_include_i(TestMsg::Child::YES);
        c->set_i(45);
        c->set_i2(45);
    }
    {
        auto c = msg_in.add_child();
        c->set_include_i(TestMsg::Child::NO);
        c->set_i(15);
        c->set_i2(15);
    }

    {
        auto c = msg_in.mutable_child2();
        c->set_include_i(TestMsg::Child2::NO);
        c->set_i(13);
    }
    {
        auto c = msg_in.mutable_child3();
        c->set_include_i(TestMsg::Child3::YES);
        c->set_i(14);

        auto sc = c->mutable_subchild();
        sc->set_include_i(TestMsg::Child2::YES);
        sc->set_i(15);
    }

    codec.info(msg_in.GetDescriptor());

    std::cout << "Message in:\n" << msg_in.DebugString() << std::endl;

    codec.load(msg_in.GetDescriptor());

    std::cout << "Try encode..." << std::endl;
    std::string bytes;
    codec.encode(&bytes, msg_in);
    std::cout << "... got bytes (hex): " << dccl::hex_encode(bytes) << std::endl;

    std::cout << "Try decode..." << std::endl;

    // b is omitted
    msg_in.clear_b();

    // d[3] is omitted
    msg_in.mutable_d()->erase(msg_in.mutable_d()->begin() + 3);

    // child[0].i is omitted
    msg_in.mutable_child(0)->clear_i();
    msg_in.mutable_child(0)->clear_i2();

    // child[2].i is omitted
    msg_in.mutable_child(2)->clear_i();
    msg_in.mutable_child(2)->clear_i2();

    msg_in.mutable_child2()->clear_i();

    decode_check(bytes);

    std::cout << "all tests passed" << std::endl;
}

void decode_check(const std::string& encoded)
{
    TestMsg msg_out;
    codec.decode(encoded, &msg_out);

    std::cout << "... got Message out:\n" << msg_out.DebugString() << std::endl;

    assert(msg_in.SerializeAsString() == msg_out.SerializeAsString());
}
