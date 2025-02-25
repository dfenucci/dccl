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
#ifndef DCCLFIELDCODECDEFAULTMESSAGEV420210701H
#define DCCLFIELDCODECDEFAULTMESSAGEV420210701H

#include <unordered_map>

#include "dccl/field_codec.h"
#include "dccl/field_codec_manager.h"
#include "dccl/oneof.h"

#include "dccl/option_extensions.pb.h"

namespace dccl
{
namespace v4
{
/// \brief Provides the default codec for encoding a base Google Protobuf message or an embedded message by calling the appropriate field codecs for every field.
class DefaultMessageCodec : public FieldCodecBase
{
  private:
    void any_encode(Bitset* bits, const boost::any& wire_value);
    void any_decode(Bitset* bits, boost::any* wire_value);
    unsigned max_size();
    unsigned min_size();
    unsigned any_size(const boost::any& wire_value);

    boost::shared_ptr<FieldCodecBase> find(const google::protobuf::FieldDescriptor* field_desc)
    {
        return FieldCodecManager::find(field_desc, has_codec_group(), codec_group());
    }

    bool is_optional() { return this_field() && this_field()->is_optional() && !use_required(); }

    void validate();
    std::string info();
    bool check_field(const google::protobuf::FieldDescriptor* field);

    struct Size
    {
        static void repeated(boost::shared_ptr<FieldCodecBase> codec, unsigned* return_value,
                             const std::vector<boost::any>& field_values,
                             const google::protobuf::FieldDescriptor* field_desc)
        {
            codec->field_size_repeated(return_value, field_values, field_desc);
        }

        static void single(boost::shared_ptr<FieldCodecBase> codec, unsigned* return_value,
                           const boost::any& field_value,
                           const google::protobuf::FieldDescriptor* field_desc)
        {
            if (!is_part_of_oneof(field_desc))
                codec->field_size(return_value, field_value, field_desc);
            else
            {
                // If the field belongs to a oneof, do nothing if the value is empty,
                // or add the size of the fiels as if it were required otherwise
                if (!field_value.empty())
                    codec->field_size(return_value, field_value, field_desc);
            }
        }

        static void oneof(unsigned* return_value,
                          const google::protobuf::OneofDescriptor* oneof_desc,
                          const google::protobuf::Message&)
        {
            // Add the bits needed to encode the case enumerator
            *return_value += oneof_size(oneof_desc);
        }
    };

    struct Encoder
    {
        static void repeated(boost::shared_ptr<FieldCodecBase> codec, Bitset* return_value,
                             const std::vector<boost::any>& field_values,
                             const google::protobuf::FieldDescriptor* field_desc)
        {
            codec->field_encode_repeated(return_value, field_values, field_desc);
        }

        static void single(boost::shared_ptr<FieldCodecBase> codec, Bitset* return_value,
                           const boost::any& field_value,
                           const google::protobuf::FieldDescriptor* field_desc)
        {
            if (!is_part_of_oneof(field_desc))
                codec->field_encode(return_value, field_value, field_desc);
            else
            {
                // If the field belongs to a oneof, do nothing if the value is empty,
                // or encode the fiels as if it were required otherwise
                if (!field_value.empty())
                    codec->field_encode(return_value, field_value, field_desc);
            }
        }

        static void oneof(Bitset* return_value, const google::protobuf::OneofDescriptor* oneof_desc,
                          const google::protobuf::Message& msg)
        {
            // Encode 0 if oneof is not set, the index of the field set + 1 otherwise
            unsigned case_ = 0;
            auto refl = msg.GetReflection();
            if (refl->HasOneof(msg, oneof_desc))
                for (auto i = 0; i < oneof_desc->field_count(); ++i)
                    if (refl->HasField(msg, oneof_desc->field(i)))
                    {
                        case_ = i + 1;
                        break;
                    }

            Bitset case_bits(oneof_size(oneof_desc), case_);
            return_value->append(case_bits);
        }
    };

    struct MaxSize
    {
        // Keeps track of the maximum size of each oneof
        static std::unordered_map<std::string, unsigned> oneofs_max_size;

        static void field(boost::shared_ptr<FieldCodecBase> codec, unsigned* return_value,
                          const google::protobuf::FieldDescriptor* field_desc)
        {
            if (!is_part_of_oneof(field_desc))
                codec->field_max_size(return_value, field_desc);
            else
            {
                // For oneof fields the max size is the size of the largest field belonging
                // to the oneof itself, considering all of them required.

                // Get the max size of the field
                auto fld_max_size = 0u;
                codec->field_max_size(&fld_max_size, field_desc);

                auto parent_oneof_name = field_desc->containing_oneof()->full_name();

                // Calculate the maximum between the field's size and the latest maximum size stored
                auto new_max_size = std::max(fld_max_size, oneofs_max_size[parent_oneof_name]);

                // Add the difference between the new and the old max size to the result and store the
                // new max size
                *return_value += (new_max_size - oneofs_max_size[parent_oneof_name]);
                oneofs_max_size[parent_oneof_name] = new_max_size;
            }
        }

        static void oneof(unsigned* return_value,
                          const google::protobuf::OneofDescriptor* oneof_desc)
        {
            // Add the bits needed to encode the case enumerator
            *return_value += oneof_size(oneof_desc);
            // Add the maximum size among the fields (0 if not initialised0
            *return_value += oneofs_max_size[oneof_desc->full_name()];
        }
    };

    struct MinSize
    {
        static void field(boost::shared_ptr<FieldCodecBase> codec, unsigned* return_value,
                          const google::protobuf::FieldDescriptor* field_desc)
        {
            // defer minimum size calculation for dynamic conditions (since omit == 0)
            // this may provide an incorrect min_size() value for reporting but allows
            // use of index based lua scripts in dynamic conditions
            DynamicConditions& dc = codec->dynamic_conditions(field_desc);
            if (dc.has_omit_if() || dc.has_required_if())
                *return_value = 0;
            // Minimum size of a field belonging to oneof is zero
            else if (!is_part_of_oneof(field_desc))
                codec->field_min_size(return_value, field_desc);
        }

        static void oneof(unsigned* return_value,
                          const google::protobuf::OneofDescriptor* oneof_desc)
        {
            // Add the bits needed to encode the case enumerator
            *return_value += oneof_size(oneof_desc);
        }
    };

    struct Validate
    {
        static void field(boost::shared_ptr<FieldCodecBase> codec, bool* return_value,
                          const google::protobuf::FieldDescriptor* field_desc)
        {
            codec->field_validate(return_value, field_desc);
        }

        static void oneof(bool*, const google::protobuf::OneofDescriptor*)
        { /* Do nothing */
        }
    };

    struct Info
    {
        static void field(boost::shared_ptr<FieldCodecBase> codec, std::stringstream* return_value,
                          const google::protobuf::FieldDescriptor* field_desc)
        {
            if (!is_part_of_oneof(field_desc))
                codec->field_info(return_value, field_desc);
        }

        static void oneof(std::stringstream* return_value,
                          const google::protobuf::OneofDescriptor* oneof_desc)
        {
            // Do nothing if the oneof descriptor is null
            if (!oneof_desc)
                return;

            // Print it otherwise
            internal::MessageStack msg_handler;
            std::stringstream ss;
            int depth = msg_handler.count();

            std::string name = boost::lexical_cast<std::string>(oneof_desc->index()) + ". " +
                               oneof_desc->name() + " [oneof]";

            // Calculate indentation
            const int spaces = 8;
            std::string indent = std::string(spaces * (depth), ' ');
            const int full_width = 40;

            int width = full_width - name.size();

            std::stringstream range;
            unsigned max_sz = 0, min_sz = 0;
            MaxSize::oneof(&max_sz, oneof_desc);
            MinSize::oneof(&min_sz, oneof_desc);
            range << min_sz << "-" << max_sz;

            ss << indent << name << std::setfill('.') << std::setw(std::max(1, width))
               << range.str();

            *return_value << ss.str() << " {\n";
            // Add oneof field's info
            for (auto i = 0; i < oneof_desc->field_count(); ++i)
            {
                auto codec =
                    FieldCodecManager::find(oneof_desc->field(i), FieldCodecBase::has_codec_group(),
                                            FieldCodecBase::codec_group());
                codec->field_info(return_value, oneof_desc->field(i));
            }

            *return_value << indent << "}\n";
        }
    };

    template <typename Action, typename ReturnType>
    void traverse_descriptor(ReturnType* return_value)
    {
        const google::protobuf::Descriptor* desc = FieldCodecBase::this_descriptor();

        // First, process the oneof definitions...
        for (auto i = 0, n = desc->oneof_decl_count(); part() != HEAD && i < n; ++i)
            Action::oneof(return_value, desc->oneof_decl(i));

        // ... then, process the fields
        for (int i = 0, n = desc->field_count(); i < n; ++i)
        {
            const google::protobuf::FieldDescriptor* field_desc = desc->field(i);

            if (!check_field(field_desc))
                continue;

            Action::field(find(field_desc), return_value, field_desc);
        }
    }

    template <typename Action, typename ReturnType>
    ReturnType traverse_const_message(const boost::any& wire_value)
    {
        try
        {
            ReturnType return_value = ReturnType();

            const google::protobuf::Message* msg =
                boost::any_cast<const google::protobuf::Message*>(wire_value);
            const google::protobuf::Descriptor* desc = msg->GetDescriptor();
            const google::protobuf::Reflection* refl = msg->GetReflection();

            // First, process the oneof definitions...
            for (auto i = 0, n = desc->oneof_decl_count(); part() != HEAD && i < n; ++i)
                Action::oneof(&return_value, desc->oneof_decl(i), *msg);

            // ... then, process the fields
            for (int i = 0, n = desc->field_count(); i < n; ++i)
            {
                const google::protobuf::FieldDescriptor* field_desc = desc->field(i);

                if (!check_field(field_desc))
                    continue;

                boost::shared_ptr<FieldCodecBase> codec = find(field_desc);
                boost::shared_ptr<internal::FromProtoCppTypeBase> helper =
                    internal::TypeHelper::find(field_desc);

                if (field_desc->is_repeated())
                {
                    std::vector<boost::any> field_values;
                    for (int j = 0, m = refl->FieldSize(*msg, field_desc); j < m; ++j)
                        field_values.push_back(helper->get_repeated_value(field_desc, *msg, j));

                    Action::repeated(codec, &return_value, field_values, field_desc);
                }
                else
                {
                    // singular field dynamic conditions - repeated fields handled in any_encode_repeated
                    DynamicConditions& dc = dynamic_conditions(field_desc);
                    if (dc.has_omit_if())
                    {
                        // expensive, so don't do this unless we're going to use it
                        dc.regenerate(this_message(), root_message());
                        if (dc.omit())
                            continue;
                    }

                    Action::single(codec, &return_value, helper->get_value(field_desc, *msg),
                                   field_desc);
                }
            }
            return return_value;
        }
        catch (boost::bad_any_cast& e)
        {
            throw(Exception("Bad type given to traverse const, expecting const "
                            "google::protobuf::Message*, got " +
                            std::string(wire_value.type().name())));
        }
    }
};

} // namespace v4
} // namespace dccl

//encode, size, etc.
#endif
