//#include <QApplication>
#include <boost/program_options.hpp>
#include <fc/log.hpp>
#include <tornet/db/chunk.hpp>
#include <boost/exception/all.hpp>
#include <tornet/db/peer.hpp>
#include <fc/asio.hpp>
#include <boost/date_time/posix_time/posix_time.hpp>
#include <tornet/net/node.hpp>
#include <tornet/net/udt_channel.hpp>
#include <tornet/services/calc.hpp>
#include <tornet/services/chat.hpp>
#include <tornet/rpc/client.hpp>

#include <tornet/net/kad.hpp>
//#include <tornet/services/chunk_search.hpp>
#include <scrypt/bigint.hpp>

#include <fc/thread.hpp>
#include <fc/signals.hpp>
#include <tornet/rpc/service.hpp>
#include <tornet/services/accounting.hpp>

//#include <ltl/rpc/reflect.hpp>

//#include <tornet/services/chunk_service.hpp>

void handle_sigint( int si );


boost::asio::ip::udp::endpoint to_endpoint( const std::string& str ) {
      boost::asio::ip::udp::resolver r( boost::cmt::asio::default_io_service() );
      std::string host = str.substr( 0, str.find(':') );
      std::string port_id = str.substr( str.find(':')+1, str.size());
      std::string port = port_id.substr(0, port_id.find('-') );
      std::string id   = port_id.substr(port_id.find('-')+1, port_id.size() );
      std::vector<boost::asio::ip::udp::endpoint> eps = boost::cmt::asio::udp::resolve( r,host,port);
      return eps.front();
}

enum chunk_order{
  by_revenue,
  by_opportunity,
  by_distance
};

void print_peers( const tornet::db::peer::ptr& peers ) {

}

void print_chunks( const tornet::db::chunk::ptr& db, int start, int limit, chunk_order order ) {

    int cnt = db->count();
    if( order == by_distance ) {
    std::cerr<<std::setw(6)<<"Index"<<"  "<<std::setw(40)<< "ID" << "  " << std::setw(8) << "Size" << "  " 
            <<std::setw(8) << "Queries"  << "  " 
            <<std::setw(10)<< "Q/Year"   << "  "
            <<std::setw(5) << "DistR"    << "  " 
            <<std::setw(8) << "Price"    << "  " 
            <<"Rev/Year\n";
    std::cerr<<"------------------------------------------------------------------------------------\n";
        tornet::db::chunk::meta m;
        scrypt::sha1 id;
        for( uint32_t i = 0; i <  cnt; ++i ) {
          db->fetch_index( i+1, id, m );
          std::cerr<< std::setw(6) << i                        << "  "
                   <<                fc::string(id).c_str()    << "  " 
                   << std::setw(8)  << m.size                  << "  " 
                   << std::setw(8)  << m.query_count           << "  " 
                   << std::setw(10) << m.annual_query_count()  << "  "
                   << std::setw(5)  << (int)m.distance_rank    << "  " 
                   << std::setw(8)  << m.price()               << "  "
                   << std::setw(10) << m.annual_revenue_rate() << "\n";
        }
    } else if( order == by_opportunity ) {
    std::cerr<<std::setw(6)<<"Index"<<"  "<<std::setw(40)<< "ID" << "  " << std::setw(8) << "Size" << "  " 
            <<std::setw(8) << "Queries"  << "  " 
            <<std::setw(10)<< "Q/Year"   << "  "
            <<std::setw(5) << "DistR"    << "  " 
            <<std::setw(8) << "Price"    << "  " 
            <<"Rev/Year*\n";
    std::cerr<<"------------------------------------------------------------------------------------\n";
      std::vector<tornet::db::chunk::meta_record> recs;
      db->fetch_best_opportunity( recs, 10 );
      for( uint32_t i = 0; i < recs.size(); ++i ) {
          std::cerr<< std::setw(6) << i                        << "  "
                   <<                fc::string(recs[i].key).c_str()                      << "  " 
                   << std::setw(8)  << recs[i].value.size                  << "  " 
                   << std::setw(8)  << recs[i].value.query_count           << "  " 
                   << std::setw(10) << recs[i].value.annual_query_count()  << "  "
                   << std::setw(5)  << (int)recs[i].value.distance_rank    << "  " 
                   << std::setw(8)  << recs[i].value.price()               << "  "
                   << std::setw(10) << recs[i].value.annual_revenue_rate() << "\n";
      }
    } else if( order == by_revenue ) {
      std::vector<tornet::db::chunk::meta_record> recs;
      db->fetch_worst_performance( recs, 10 );
      for( uint32_t i = 0; i < recs.size(); ++i ) {
          std::cerr<< std::setw(6) << i                        << "  "
                   <<                recs[i].key                      << "  " 
                   << std::setw(8)  << recs[i].value.size                  << "  " 
                   << std::setw(8)  << recs[i].value.query_count           << "  " 
                   << std::setw(10) << recs[i].value.annual_query_count()  << "  "
                   << std::setw(5)  << (int)recs[i].value.distance_rank    << "  " 
                   << std::setw(8)  << recs[i].value.price()               << "  "
                   << std::setw(10) << recs[i].value.annual_revenue_rate() << "\n";
      }
    }
}

void print_record_header() {
    std::cerr<<std::setw(21) <<"Host:Port"<<" "
             <<std::setw(40) <<"ID"<<" "
             <<std::setw(10) <<"Dist"<<" "
             <<std::setw(5)  <<"Rank"<<" "
             <<std::setw(5)  <<"PRank"<<" "
             <<std::setw(8)  <<"% Avail"<<" "
             <<std::setw(8)  <<"RTT(us)"<<" "
             <<std::setw(10) <<"EstB(B/s)"<<" "
             <<std::setw(10) <<"SentC"<<" "
             <<std::setw(10) <<"RecvC"<<" "
             <<std::setw(3)  <<"FW"<<" "
             <<std::setw(30) <<"First Contact"<<" "
             <<std::setw(30) <<"Last Contact"<<" "
             <<std::setw(10) <<"nonce[0]"<<" "
             <<std::setw(10) <<"nonce[1]"<<" "
             <<std::endl;
   std::cerr<<"----------------------------------------------------------"
            <<"----------------------------------------------------------"
            <<"----------------------------------------------------------\n";
}
int calc_dist( const scrypt::sha1& d ) {
  scrypt::bigint bi((char*)d.hash, sizeof(d.hash));
  scrypt::sha1   mx; memset( mx.hash,0xff,sizeof(mx.hash));
  scrypt::bigint max((char*)mx.hash,sizeof(mx.hash));

  return ((bi * scrypt::bigint( 1000 )) / max).to_int64();
}


boost::posix_time::ptime to_system_time( uint64_t utc_us ) {
//    typedef boost::chrono::microseconds duration_t;
//    typedef duration_t::rep rep_t;
//    rep_t d = boost::chrono::duration_cast<duration_t>(t.time_since_epoch()).count();
    static boost::posix_time::ptime epoch(boost::gregorian::date(1970, boost::gregorian::Jan, 1));
    return epoch + boost::posix_time::seconds(long(utc_us/1000000)) + boost::posix_time::microseconds(long(utc_us%1000000));
}

void print_record( const tornet::db::peer::record& rec, const tornet::node::ptr& nd ) {
  std::cerr<< std::setw(21) 
           << (boost::asio::ip::address_v4(rec.last_ip).to_string()+":"+boost::lexical_cast<std::string>(rec.last_port))<<" "
           << rec.id() << " "
           << std::setw(10)  << calc_dist(rec.id() ^ nd->get_id()) << " "
           << std::setw(5)   << int(rec.rank) << " "
           << std::setw(5)   << int(rec.published_rank) << " "
           << std::setw(8)   << double(rec.availability) / uint16_t(0xffff) <<" "
           << std::setw(8)   << (rec.avg_rtt_us) <<" "
           << std::setw(10)  << rec.est_bandwidth << " "
           << std::setw(10)  << rec.sent_credit << " "
           << std::setw(10)  << rec.recv_credit << " "
           << std::setw(3)   << (rec.firewalled ? 'Y' : 'N') << " "
           << std::setw(30)  << boost::posix_time::to_simple_string(to_system_time( rec.first_contact )) <<" "
           << std::setw(30)  << boost::posix_time::to_simple_string(to_system_time( rec.last_contact ))  <<" "
           << std::setw(10)  << (rec.nonce[0]) << " " << std::setw(10) << (rec.nonce[1]) << " "
           << std::endl;
}


void cli( const chunk_service::ptr& cs, const tornet::node::ptr& nd ) {
  try {
     std::string line;
     while( std::getline(std::cin, line) ) {
       std::stringstream ss(line);
     
       std::string cmd;
       ss >> cmd;
     
       if( cmd == "import" ) {
         std::string infile;
         ss >> infile;
         scrypt::sha1 tid, check;
         cs->import( infile, tid, check );
         std::cout<<"Created "<<infile<<".tornet with TID: "<<std::string(tid)<<" and CHECK: "<<std::string(check)<<std::endl;
       } else if( cmd == "export" ) {
         std::string infile;
         ss >> infile;

       } else if( cmd == "export_tid" ) {
         std::string tid,check;
         ss >> tid >> check;
         cs->export_tornet( scrypt::sha1(tid), scrypt::sha1(check) );
       } else if( cmd == "publish" ) {
         std::string tid, check;
         ss >> tid >> check;
         cs->publish_tornet( scrypt::sha1(tid), scrypt::sha1(check), 3 );
       } else if( cmd == "stop" ) {
         std::string what;
         ss >> what;
         if( what == "pub" ) {
          slog( "Stoping Publishing" );
          cs->enable_publishing( false );
         }
       } else if( cmd == "start" ) {
         std::string what;
         ss >> what;
         if( what == "pub" ) {
          slog( "Starting Publishing" );
          cs->enable_publishing( true );
         }

       } else if( cmd == "find" ) {
         std::string tid;
         ss >> tid;

         slog( "Starting search..." );
         tornet::chunk_search::ptr csearch(  boost::make_shared<tornet::chunk_search>(nd, scrypt::sha1(tid), 10, 1, true ) );  
         csearch->start();
         csearch->wait();

         slog( "Results: \n" );
         const std::map<tornet::node::id_type,tornet::node::id_type>&  r = csearch->current_results();
         std::map<tornet::node::id_type,tornet::node::id_type>::const_iterator itr  = r.begin(); 
         while( itr != r.end() ) {
           slog( "   node id: %s   distance: %s", 
                     string(itr->first).c_str(), string(itr->second).c_str() );
           ++itr;
         }

         slog( "Hosting Matches: \n" ); {
           const std::map<tornet::node::id_type,tornet::node::id_type>&  r = csearch->hosting_nodes();
           std::map<tornet::node::id_type,tornet::node::id_type>::const_iterator itr  = r.begin(); 
           while( itr != r.end() ) {
             slog( "   node id: %2%   distance: %1%", 
                     //  itr->first, itr->second );
                     string(itr->first).c_str(), string(itr->second).c_str() );
             ++itr;
           }
         }

         wlog( "avg query rate: %1%    weight: %2%", csearch->avg_query_rate(), csearch->query_rate_weight() );

     
       } else if( cmd == "show" ) {
         std::string what;
         std::string order;
         ss >> what;
         ss >> order;
         chunk_order o = by_distance;
         if( order == "by_revenue" ) o = by_revenue;
         else if( order == "by_opportunity" ) o = by_opportunity;
         else if( order == "by_distance" || order == "" ) o = by_distance;
         else { 
          std::cerr<<"Invalid order '"<<order<<"'\n  options are by_revenue, by_opportunity, by_distance\n";
         }

         if( what == "rank" ) {
          std::cerr<<"Local Rank: "<< nd->rank()<<std::endl;
         }

         if( what == "local" ) {
          print_chunks( cs->get_local_db(), 0, 0xffffff, o );
         } else if( what == "cache") {
          print_chunks( cs->get_cache_db(), 0, 0xffffff, o );
          /*
            int cnt = cs->get_cache_db()->count();
            tornet::db::chunk::meta m;
            scrypt::sha1 id;
            std::cerr<<"Index "<< "ID" << "  " << "Size" << "  " << "Queried" <<  "  " << "Dist Rank" << "  Annual Rev\n";
            std::cerr<<"----------------------------------------------------------------------\n";
            for( uint32_t i = 0; i <  cnt; ++i ) {
              cs->get_cache_db()->fetch_index( i+1, id, m );
              std::cerr<<i<<"] " << id << "  " << m.size << "  " << m.query_count <<  "  " << (int)m.distance_rank << "  " << m.annual_revenue_rate() << "\n";
            }
          */
         } else if( what == "connections" ) {
            std::vector<tornet::db::peer::record> recs = nd->active_peers();
            print_record_header();
            for( uint32_t i =0; i < recs.size(); ++i ) {
              print_record( recs[i], nd );
            }
         } else if( what == "publish" ) {
            uint32_t cnt = cs->get_publish_db()->count(); 
            tornet::db::publish::record rec;
            scrypt::sha1                id;
            std::cerr<< std::setw(40) << "ID" << " "
                     << std::setw(10) << "AI" << " "
                     << std::setw(10) << "Next" << " "
                     << std::setw(10) << "Hosts" << " "
                     << std::setw(10) << "Desired Hosts" << "\n";
            std::cerr<<"-----------------------------------------------------------------------------\n";
            for( uint32_t i = 1; i <= cnt; ++i ) {
              cs->get_publish_db()->fetch_index( i, id, rec );
              std::cerr << id << " " 
                        << std::setw(10) << rec.access_interval << " " 
                        << std::setw(10) << (rec.next_update == 0 ? std::string("now") : boost::posix_time::to_simple_string( to_system_time( rec.next_update ) )) << " "
                        << std::setw(5)  << rec.host_count << " " 
                        << std::setw(5)  << rec.desired_host_count << std::endl;
            }
         
         } else if( what == "users" ) {
            print_record_header();
            tornet::db::peer::ptr p = nd->get_peers();
            int cnt = p->count();
            tornet::db::peer::record r;
            scrypt::sha1 rid;
            for( uint32_t i = 0; i < cnt; ++i ) {
              p->fetch_index( i+1, rid, r );
              print_record( r, nd );
            }
         } else if( what == "tornet" ) {
           std::string tornet_file;
           ss >> tornet_file;
         } else if( what == "tid" ) {
           std::string tid,check; 
           ss >> tid >> check;
         }
     
       } 
       else if( cmd == "rankeffort" ) {
           std::string per;
           ss >> per;
           nd->start_rank_search( boost::lexical_cast<double>(per) / 100.0 );
       }
       else {
         std::cerr<<"\nCommands:\n";
         std::cerr<<"  import      FILENAME               - loads FILENAME and creates chunks and dumps FILENAME.tornet\n";
         std::cerr<<"  export      TORNET_FILE            - loads TORNET_FILE and saves FILENAME\n";
         std::cerr<<"  find        CHUNK_ID               - looks for the chunk and returns query rate stats for the chunk\n";
         std::cerr<<"  export_tid  TID CHECKSUM [OUT_FILE]- loads TORNET_FILE and saves FILENAME\n";
         std::cerr<<"  publish TID CHECKSUM                - pushes chunks from TORNET_FILE out to the network, including the TORNETFILE itself\n";
         std::cerr<<"  show local START LIMIT [by_distance|by_revenue|by_opportunity]\n";
         std::cerr<<"  show cache START LIMIT |by_distance|by_revenue|by_opportunity]\n";
         std::cerr<<"  show users START LIMIT |by_balance|by_rank|...]\n";
         std::cerr<<"  show publish \n";
         std::cerr<<"  rankeffort EFFORT                  - percent effort to apply towoard improving rank\n";
         std::cerr<<"  help                               - prints this menu\n\n";
       }
     }
  } catch ( const boost::exception& e ) {
    elog( "%s", boost::diagnostic_information(e) );
  } catch ( const std::exception& e ) {
    elog( "%s", boost::diagnostic_information(e) );
  }
}

void start_services( int argc, char** argv ) {
  slog( "starting services..." );
      std::string data_dir;
      uint16_t    node_port = 0;
      std::vector<std::string> init_connections;

      namespace po = boost::program_options;
      po::options_description desc("Allowed options");
      desc.add_options()
        ("help,h", "print this help message." )
        ("listen,l", po::value<uint16_t>(&node_port)->default_value(node_port), "Port to run node on." )
        ("data_dir,d", po::value<std::string>(&data_dir)->default_value("data"), "Directory to store data" )
        ("bootstrap,b", po::value<std::vector<std::string> >(&init_connections), "HOST:PORT of a bootstrap node" )
      ;
      po::variables_map vm;
      po::store( po::parse_command_line(argc,argv,desc), vm );
      po::notify(vm);

      if( vm.count("help") ) {
          std::cout << desc << std::endl;
          QCoreApplication::exit(1);
          exit(1);
          return;
      }
      
      tornet::node::ptr node(new tornet::node() );
  try {
      node->init( data_dir, node_port );
      
      tornet::rpc::service::ptr        srv( new tornet::service::calc_service( node, "rpc", 102 ) );
      chunk_service::ptr               cs(  new chunk_service( data_dir+"/chunks", node, "chunks", 100 ) );
  //    tornet::service::accounting::ptr as(  new tornet::service::accounting( data_dir + "/accounting", node, "accounting", 69 ) );

      new boost::thread( boost::bind( cli, cs, node ) );

      for( uint32_t i = 0; i < init_connections.size(); ++i ) {
        try {
            wlog( "Attempting to connect to %1%", init_connections[i] );
            scrypt::sha1 id = node->connect_to( to_endpoint(init_connections[i]) );
            wlog( "Connected to %1%", id );

            slog( "Starting bootstrap process by searching for my node id + 1" );
            scrypt::sha1 sh;
            sh.hash[4] = 0x01000000;
            scrypt::sha1 target = sh ^ node->get_id();
            tornet::kad_search::ptr ks( new tornet::kad_search( node, target ) );
            ks->start();
            ks->wait();

            slog( "Results: \n" );
            const std::map<tornet::node::id_type,tornet::node::id_type>&  r = ks->current_results();
            std::map<tornet::node::id_type,tornet::node::id_type>::const_iterator itr  = r.begin(); 
            while( itr != r.end() ) {
              slog( "   node id: %2%   distance: %1%", 
                        itr->first, itr->second );
              ++itr;
            }
        } catch ( const boost::exception& e ) {
            wlog( "Unable to connect to node %1%, %2%", init_connections[i], boost::diagnostic_information(e) ) ;
        }
      }
    wlog( "exec... " );
      boost::cmt::exec();
    wlog( "done exec..." );
  } catch ( const boost::exception& e ) {
    elog( "%1%", boost::diagnostic_information(e) );
  } catch ( const std::exception& e ) {
    elog( "%1%", boost::diagnostic_information(e) );
  } catch ( ... ) {
    elog( "unhandled exceptioN!" );
  }
  elog( "gracefully shutdown node." );
  node->close();
  wlog( "\n\nexiting services!\n\n" );
}



int main( int argc, char** argv ) {
  signal( SIGINT, handle_sigint );
  boost::cmt::thread::current().set_name("main");
  try { 
      boost::cmt::thread* sthread = boost::cmt::thread::create("service");
      //QApplication* app = new QApplication(argc, argv);

      sthread->async( boost::bind(&start_services,argc,argv) );
      //app->exec();

      //quit_signal();
      slog( "wating for services to clean up" );
      sthread->quit();
      wlog( "wait for services..." );
//      usleep(1000*1000);
  } catch ( const boost::exception& e ) {
    elog( "%1%", boost::diagnostic_information(e)  );
    return -1;
  } catch ( const std::exception& e ) {
    elog( "%1%", boost::diagnostic_information(e)  );
    return -1;
  } catch ( ... ) {
    elog( "Unknown exception" );
    return -1;
  }
  slog( "Exiting cleanly" );
  return 0;
}
void handle_sigint( int si ) {
  slog( "%1%", si );
  static int count = 0;
  if( !count ){ 
    QCoreApplication::exit(0);
  }
  else exit(si);
  ++count;
}
