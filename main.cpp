#define MCAP_IMPLEMENTATION
#define DEFAULT_TOPIC_QOS 1

#include <chrono>
#include <csignal>
#include <mcap/writer.hpp>
#include <mqtt/client.h>
#include <string>
#include <tuple>
#include <unordered_map>

#include <CLI/App.hpp>
#include <CLI/Config.hpp>
#include <CLI/Formatter.hpp>

using namespace std::chrono_literals;

std::atomic_bool signal_quit(false);

void SignalHandler(int signal)
{
    std::cout << "Quitting..." << std::endl;
    signal_quit.store(true);
}

// Return the stamp property if set, otherwise use the current time
mcap::Timestamp GetTimestampFromProperties(
    mqtt::properties properties, const std::string& stamp_property)
{
    if (!stamp_property.empty())
    {
        size_t property_count = properties.count(mqtt::property::USER_PROPERTY);
        for (size_t i = 0; i < property_count; ++i)
        {
            auto property = properties.get(mqtt::property::USER_PROPERTY, i);
            const auto& [property_name, property_value] = mqtt::get<mqtt::string_pair>(property);
            if (property_name == stamp_property)
            {
                return std::stoull(property_value);
            }
        }
    }

    return std::chrono::duration_cast<std::chrono::nanoseconds>(
        std::chrono::system_clock::now().time_since_epoch())
        .count();
}

int main(int argc, char** argv)
{
    // Set up signal handler
    signal(SIGINT, SignalHandler);

    // Parse command line arguments
    CLI::App app;
    std::string hostname = "tcp://localhost:1883";
    app.add_option("--host", hostname, "Broker url");

    std::string output_path = "output.mcap";
    app.add_option("-o,--output", output_path, "Output path");

    std::vector<std::string> topic_patterns_to_encodings_args;
    app.add_option(
           "--topics", topic_patterns_to_encodings_args, "Topics and message encoding split by :")
        ->required(true);

    std::string stamp_property;
    app.add_option("--stamp-property", stamp_property,
        "Name of the MQTT user property to use as publish time in nanoseconds (default empty)");

    CLI11_PARSE(app, argc, argv);

    std::unordered_map<std::string, std::string> topic_patterns_to_encodings;

    // Assert topics to encodings
    for (const auto& arg : topic_patterns_to_encodings_args)
    {
        // Split by colon
        auto pos = arg.find(':');
        if (pos == std::string::npos)
        {
            // No encoding specified in the argument
            topic_patterns_to_encodings.emplace(arg, "");
        }
        else
        {
            // Encoding specified in the argument
            topic_patterns_to_encodings.emplace(arg.substr(0, pos), arg.substr(pos + 1));
        }
    }

    // Set up MQTT client
    auto create_options = mqtt::create_options_builder().mqtt_version(MQTTVERSION_5).finalize();
    auto mqtt_client_ = std::make_shared<mqtt::async_client>(hostname, "mqtt_mcap", create_options);
    auto connect_options = mqtt::connect_options_builder()
                               .keep_alive_interval(10s)
                               .clean_start(true)
                               .connect_timeout(1s)
                               .mqtt_version(MQTTVERSION_5)
                               .finalize();

    // In order to not miss any messages, setup client to receive messages before connecting
    mqtt_client_->start_consuming();

    // Repeatedly trying to connect until successful
    while (!signal_quit)
    {
        try
        {
            // Start connecting
            auto connect_token = mqtt_client_->connect(connect_options);
            connect_token->wait();
            break;
        }
        catch (const mqtt::exception& e)
        {
            std::cout << "MQTT connection failed: " << e.what() << std::endl;
            std::this_thread::sleep_for(1s);
        }
    }

    if (signal_quit)
    {
        return 0;
    }

    std::cout << "MQTT connection successful" << std::endl;

    // Set up MCAP writer
    mcap::McapWriter writer;
    {
        mcap::McapWriterOptions opts("mqtt");
        auto status = writer.open(output_path, opts);
        if (!status.ok())
        {
            std::cerr << "Failed to open mcap writer: " << status.message << "\n";
            throw std::runtime_error("could not open mcap writer");
        }
    }

    std::unordered_map<std::string, mcap::ChannelId> channel_map;
    std::unordered_map<size_t, std::string> subscription_id_to_encoding;
    int subscription_id = 0;

    // Set up subscribers, with incrementing subscription identifiers
    for (const auto& [topic_pattern, encoding] : topic_patterns_to_encodings)
    {
        subscription_id++;
        subscription_id_to_encoding.emplace(subscription_id, encoding);
        auto properties = mqtt::properties();
        properties.add(mqtt::property(mqtt::property::SUBSCRIPTION_IDENTIFIER, subscription_id));
        mqtt_client_->subscribe(
            topic_pattern, DEFAULT_TOPIC_QOS, mqtt::subscribe_options(), properties);
    }

    while (!signal_quit)
    {
        mqtt::const_message_ptr msg_ptr;
        if (!mqtt_client_->try_consume_message_for(&msg_ptr, 1s))
        {
            continue;
        }
        std::cout << "Received message: " << msg_ptr->get_topic() << std::endl;

        std::string topic = msg_ptr->get_topic();
        auto properties = msg_ptr->get_properties();
        auto search_it = channel_map.find(topic);
        if (channel_map.find(topic) == channel_map.end())
        {
            // Use subscription identifier to find the corresponding encoding
            std::optional<std::string> encoding;
            size_t property_count = properties.count(mqtt::property::SUBSCRIPTION_IDENTIFIER);
            for (size_t i = 0; i < property_count; ++i)
            {
                auto property = properties.get(mqtt::property::SUBSCRIPTION_IDENTIFIER, i);
                int subscription_identifier = mqtt::get<int>(property);
                if (subscription_id_to_encoding.count(subscription_identifier) == 0)
                {
                    throw std::runtime_error(
                        "received unexpected message with unknown subscription "
                        "identifier '" +
                        std::to_string(subscription_identifier) + "'");
                }
                std::string found_encoding =
                    subscription_id_to_encoding.at(subscription_identifier);
                if (encoding && encoding != found_encoding)
                {
                    throw std::runtime_error("multiple encodings found for same topic '" + topic +
                                             "': '" + *encoding + "' '" + found_encoding + "'");
                }
                encoding = found_encoding;
            }

            if (!encoding)
            {
                throw std::runtime_error(
                    "received unexpected message with no encoding found for topic '" + topic + "'");
            }

            mcap::Channel channel(topic, *encoding, 0);
            writer.addChannel(channel);
            search_it = channel_map.emplace(topic, channel.id).first;
        }
        mcap::ChannelId channel_id = search_it->second;

        const std::string& payload = msg_ptr->get_payload_str();

        // Construct a MCAP message
        mcap::Message msg;
        msg.channelId = channel_id;
        msg.data = reinterpret_cast<const std::byte*>(payload.data());
        msg.dataSize = payload.size();
        msg.logTime = GetTimestampFromProperties(properties, stamp_property);
        msg.publishTime = msg.logTime;
        auto status = writer.write(msg);

        if (!status.ok())
        {
            std::cerr << "Failed to write message: " << status.message << "\n";
            throw std::runtime_error("could not write message");
        }
    }

    writer.close();
}
