
// ============================================================ //
//
//   d88888D db   d8b   db  .d8b.  db    db
//   YP  d8' 88   I8I   88 d8' `8b `8b  d8'
//      d8'  88   I8I   88 88ooo88  `8bd8'
//     d8'   Y8   I8I   88 88~~~88    88
//    d8' db `8b d8'8b d8' 88   88    88
//   d88888P  `8b8' `8d8'  YP   YP    YP
//
//   open-source, cross-platform, crypto-messenger
//
//   Copyright (C) 2012-2016  Marc Weiler
//
// ============================================================ //

#include <boost/program_options.hpp>
#include <boost/log/expressions.hpp>
#include <boost/log/expressions/formatters/date_time.hpp>
#include <boost/log/expressions/formatters/if.hpp>
#include <boost/log/support/date_time.hpp>
#include <boost/log/utility/setup/file.hpp>
#include <boost/log/utility/setup/console.hpp>
#include <boost/log/utility/setup/common_attributes.hpp>

#include "logger.h"
#include "server.h"

#include <fstream>

namespace po = boost::program_options;
namespace logging = boost::log;
namespace keywords = boost::log::keywords;
namespace expressions = boost::log::expressions;

#define NUM_WORKERS 20
#define MAX_WORKERS 50

#define LOG_FILENAME "/var/log/zway"

boost::shared_ptr<boost::asio::io_service> io_service =
        boost::make_shared<boost::asio::io_service>();

boost::shared_ptr<boost::asio::io_service::work> work =
        boost::make_shared<boost::asio::io_service::work>(*io_service);

// ============================================================ //

void worker(boost::shared_ptr<boost::asio::io_service> io_service)
{
    boost::system::error_code ec;

    io_service->run(ec);
}

// ============================================================ //

void signal_handler(int sig)
{
	io_service->stop();
}

// ============================================================ //

int main(int argc, char** argv)
{
    // get working directory

    char *tmp = get_current_dir_name();

    std::string workingDir(tmp);

    free(tmp);

    // parse command line

    std::string address;

    int32_t port;

    int32_t numWorkers;

    po::options_description desc("Options");

    desc.add_options()
        ("help,h",
            "this help message")
        ("address,a",
            po::value<std::string>(&address), "address to bind")
        ("port,p",
            po::value<int32_t>(&port)->default_value(ZWAY_PORT), "port to use")
        ("num-workers,n",
            po::value<int32_t>(&numWorkers)->default_value(NUM_WORKERS), "number of worker threads")
        ("daemon,d",
            "start daemon");

    po::variables_map vm;

    try {

        po::store(po::parse_command_line(argc, argv, desc), vm);
    }
    catch (std::exception &e) {

        std::cerr << e.what() << "\n";

        desc.print(std::cout);

        return -1;
    }

    po::notify(vm);

    if (vm.count("help") || !vm.count("address")) {

        desc.print(std::cout);

        return -1;
    }

    if (vm.count("daemon")) {

		// fork parent process

		pid_t pid = fork();

		if (pid < 0) {

			// ...

			exit(EXIT_FAILURE);
		}

		if (pid > 0) {

			exit(EXIT_SUCCESS);
		}

		umask(0);

        // set session id

    	pid_t sid = setsid();

        if (sid < 0) {

        	exit(EXIT_FAILURE);
        }

        // write pid file

        std::ofstream ofs("/var/run/zway.pid");
        ofs << sid;
        ofs.close();

        // reset working dir

        if (chdir("/") < 0) {

        	exit(EXIT_FAILURE);
        }

        // close std fds

        close(STDIN_FILENO);
        close(STDOUT_FILENO);
        close(STDERR_FILENO);

        // set signal handler

        signal(SIGUSR1, signal_handler);

        // init file log

        logging::add_file_log(
            keywords::file_name           = LOG_FILENAME,
            keywords::auto_flush          = true,
            keywords::rotation_size       = 10 * 1024 * 1024,
          //keywords::time_based_rotation = logging::sinks::file::rotation_at_time_point(0, 0, 0),
            keywords::format              = "[%TimeStamp%]: %Message%"
        );
    }
    else {

        // init console log

        auto sink = logging::add_console_log(std::cout);

        sink->set_formatter(
            expressions::stream <<
            "\033[32m" << expressions::format_date_time<boost::posix_time::ptime>("TimeStamp", "%H:%M:%S")  << "\033[0m " <<
            "[tid=" << expressions::attr<boost::log::attributes::current_thread_id::value_type>("ThreadID") << "] " <<
            expressions::if_(expressions::is_in_range<int>("Severity", 3, 4)) [ expressions::stream << "\033[33m" ] <<
            expressions::if_(expressions::is_in_range<int>("Severity", 4, 5)) [ expressions::stream << "\033[31m" ] <<
            expressions::smessage <<
            "\033[0m"
        );
    }

    logging::add_common_attributes();

    // init worker thread pool

    boost::thread_group workers;

    for (uint32_t i=0; i<numWorkers && i<MAX_WORKERS; i++) {

        workers.create_thread(boost::bind(&worker, io_service));
    }


    FcmSender::startup();


    // init server

    Server server(io_service);

    if (!server.start(workingDir, address, port)) {

        return -1;
    }

    if (vm.count("daemon")) {

        io_service->run();
    }
    else {

        //LOG_INFO("Controls:\np -> pause/resume server\nr -> remove sessions\ni -> info\ne -> exit");

        for (;;) {

            char c = getchar();

            if (c == 'e') {

                break;
            }
            else {

            	switch (c) {

            		case 'i':

            			server.info();

            			break;

            		case 'r':

            		    server.removeSessions();

            		    break;

            		case 'p': {

            		    if (server.paused()) {

            		        server.resume();

                            LOG_INFO << "Server resumed";
            		    }
            		    else {

            		        server.pause();

                            LOG_INFO << "Server paused";
            		    }

            		    break;
            		}
            	}
            }
        }
    }

    server.close();

    // nothing to do anymore

    work.reset();

    workers.join_all();


    FcmSender::cleanup();


    LOG_INFO << "bye";

    return 0;
}

// ============================================================ //
