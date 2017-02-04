#ifndef SERVER_RESOURCE_HPP
#define	SERVER_RESOURCE_HPP
#define BOOST_SPIRIT_THREADSAFE

#include "renesolalog.h"
#include "redispp.h"
#include <boost/date_time/posix_time/posix_time.hpp>
#include <iostream>
#include <boost/bind.hpp>
#include <list>
#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/ini_parser.hpp>
#include <boost/lexical_cast.hpp>
//#include <queue>
//#include <thread>
#include <condition_variable>
#include <assert.h>

#include "hirediscommand.h"
using namespace RedisCluster;
using namespace redispp;
using namespace boost::posix_time;

#include "server_http.hpp"
#include "client_http.hpp"

#include <boost/property_tree/ptree.hpp>
#include <boost/property_tree/json_parser.hpp>
#include <boost/algorithm/string/split.hpp>
#include <boost/algorithm/string/classification.hpp>
//Added for the default_resource example
#include<fstream>
using namespace std;
//Added for the json:
using namespace boost::property_tree;

typedef SimpleWeb::Server<SimpleWeb::HTTP> HttpServer;
typedef SimpleWeb::Client<SimpleWeb::HTTP> HttpClient;
//Connection *connRedis;
string redisHost;
string redisPort;
string redisPassword;
string url;
string flow_number_param1;
string flow_number_param2;
///定义redis库
#define KV_SYS_PARAMS 0
#define KV_MF 1
#define KV_SESSION 2
#define KV_VISIT_RECORDS 3
#define KV_SHOPPING_CART 4
#define KV_OBJ_SNAPSHOT 5
#define KV_OPERATION_LOG 6
// response error code define -8001 ~ -9000 隔-10或-5
//JSON_READ_OR_WRITE_ERROR(-8010, "json read or write error", "json 格式问题")
#define JSON_READ_OR_WRITE_ERROR -8010
//CREATE_SESSION_UNKNOWN_ERROR(-8020, "create session unknown error", "创建session时未知的错误")
#define CREATE_SESSION_UNKNOWN_ERROR -8020
//CREATE_SESSION_KEY_EXIST(-8025, "key already exist when create session", "创建session时key已经存在")
#define CREATE_SESSION_KEY_EXIST -8025
//ADD_USERID_UNDER_SESSION_UNKNOWN_ERROR(-8030, "add userid unknown error", "增加userid时未知的错误")
#define ADD_USERID_UNDER_SESSION_UNKNOWN_ERROR -8030
//ADD_USERID_KEY_NOT_EXIST(-8035, "key does not exist when add userid", "增加userid时key不存在")
#define ADD_USERID_KEY_NOT_EXIST -8035
//DELETE_SESSION_UNKNOWN_ERROR(-8040, "del session unknown error", "删除session时未知的错误")
#define DELETE_SESSION_UNKNOWN_ERROR -8040
//DELETE_SESSION_KEY_NOT_EXIST(-8045, "key does not exist when del session", "删除session时key不存在")
#define DELETE_SESSION_KEY_NOT_EXIST -8045
//QUERY_SESSION_UNKNOWN_ERROR(-8050, "unknown error when query session ", "查询session时未知的错误")
#define QUERY_SESSION_UNKNOWN_ERROR -8050
//QUERY_SESSION_KEY_NOT_EXIST(-8055, "key does not exist when query session", "查询session时key不存在")
#define QUERY_SESSION_KEY_NOT_EXIST -8055
//UPDATE_SESSION_DEADLINE_UNKNOWN_ERROR(-8060, "update session unknown error", "更新session时未知的错误")
#define UPDATE_SESSION_DEADLINE_UNKNOWN_ERROR -8060
//UPDATE_SESSION_DEADLINE_KEY_NOT_EXIST(-8065, "key does not exist when update session", "更新session时key不存在")
#define UPDATE_SESSION_DEADLINE_KEY_NOT_EXIST -8065
//*********************************************************
//BATCH_CREATE_AREAS_KEY_EXIST(-8070, "key already exists when batch create areas", "批量增加地区key时key已经存在")
#define BATCH_CREATE_AREAS_KEY_EXIST -8070
//BATCH_CREATE_AREAS_UNKNOWN_ERROR(-8080, "unknown error when batch create areas", "批量增加地区key时未知错误")
#define BATCH_CREATE_AREAS_UNKNOWN_ERROR -8080
//UNKNOWN_ERROR(-8085, "unknown error", "未知错误")
#define UNKNOWN_ERROR -8085

//*********************************************************

///////////////redis cluster thread pool//////////////////////


template<typename redisConnection>
class ThreadedPool
{
    static const int poolSize_ = 10;
    typedef Cluster<redisConnection, ThreadedPool> RCluster;
    // We will save our pool in std::queue here
    typedef std::queue<redisConnection*> ConQueue;
    // Define pair with condition variable, so we can notify threads, when new connection is released from some thread
    typedef std::pair<std::condition_variable, ConQueue> ConPool;
    // Container for saving connections by their slots, just as
    typedef std::map <typename RCluster::SlotRange, ConPool*, typename RCluster::SlotComparator> ClusterNodes;
    // Container for saving connections by host and port (for redirecting)
    typedef std::map <typename RCluster::Host, ConPool*> RedirectConnections;
    // rename cluster types
    typedef typename RCluster::SlotConnection SlotConnection;
    typedef typename RCluster::HostConnection HostConnection;
    
public:
    
    ThreadedPool( typename RCluster::pt2RedisConnectFunc conn,
                 typename RCluster::pt2RedisFreeFunc disconn,
                 void* userData ) :
    data_( userData ),
    connect_(conn),
    disconnect_(disconn)
    {
    }
    
    ~ThreadedPool()
    {
        disconnect();
    }
    
    // helper function for creating connections in loop
    inline void fillPool( ConPool &pool, const char* host, int port )
    {
        for( int i = 0; i < poolSize_; ++i )
        {
            redisConnection *conn = connect_( host,
                                            port,
                                            data_ );
            
            if( conn == NULL || conn->err )
            {
                throw ConnectionFailedException();
            }
            pool.second.push( conn );
        }
    }
    
    // helper for fetching connection from pool
    inline redisConnection* pullConnection( std::unique_lock<std::mutex> &locker, ConPool &pool )
    {
        redisConnection *con = NULL;
        // here we wait for other threads for release their connections if the queue is empty
        while (pool.second.empty())
        {
            // if queue is empty here current thread is waiting for somethread to release one
            pool.first.wait(locker);
        }
        // get a connection from queue
        con = pool.second.front();
        pool.second.pop();
        
        return con;
    }
    // helper for releasing connection and placing it in pool
    inline void pushConnection( std::unique_lock<std::mutex> &locker, ConPool &pool, redisConnection* con )
    {
        pool.second.push(con);
        locker.unlock();
        // notify other threads for their wake up in case of they are waiting
        // about empty connection queue
        pool.first.notify_one();
    }
    
    // function inserts new connection by range of slots during cluster initialization
    inline void insert( typename RCluster::SlotRange slots, const char* host, int port )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        
        ConPool* &pool = nodes_[slots];
        pool = new ConPool();
        fillPool(*pool, host, port);
    }
    
    // function inserts or returning existing one connection used for redirecting (ASKING or MOVED)
    inline HostConnection insert( string host, string port )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        string key( host + ":" + port );
        HostConnection con = { key, NULL };
        try
        {
            con.second = pullConnection( locker, *connections_.at( key ) );
        }
        catch( const std::out_of_range &oor )
        {
        }
        // create new connection in case if we didn't redirecting to this
        // node before
        if( con.second == NULL )
        {
            ConPool* &pool = connections_[key];
            pool = new ConPool();
            fillPool(*pool, host.c_str(), std::stoi(port));
            con.second = pullConnection( locker, *pool );
        }
        
        return con;
    }
    
    
    inline SlotConnection getConnection( typename RCluster::SlotIndex index )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        
        typedef typename ClusterNodes::iterator iterator;
        iterator found = DefaultContainer<redisConnection>::template searchBySlots(index, nodes_);
        
        return { found->first, pullConnection( locker, *found->second ) };
    }
    
    // this function is invoked when library whants to place initial connection
    // back to the storage and the connections is taken by slot range from storage
    inline void releaseConnection( SlotConnection conn )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        pushConnection( locker, *nodes_[conn.first], conn.second );
    }
    // same function for redirection connections
    inline void releaseConnection( HostConnection conn )
    {
        std::unique_lock<std::mutex> locker(conLock_);
        pushConnection( locker, *connections_[conn.first], conn.second );
    }
    
    // disconnect both thread pools
    inline void disconnect()
    {
        disconnect<ClusterNodes>( nodes_ );
        disconnect<RedirectConnections>( connections_ );
    }
    
    template <typename T>
    inline void disconnect(T &cons)
    {
        std::unique_lock<std::mutex> locker(conLock_);
        if( disconnect_ != NULL )
        {
            typename T::iterator it(cons.begin()), end(cons.end());
            while ( it != end )
            {
                for( int i = 0; i < poolSize_; ++i )
                {
                    // pullConnection will wait for all connection
                    // to be released
                    disconnect_( pullConnection( locker, *it->second) );
                }
                if( it->second != NULL )
                {
                    delete it->second;
                    it->second = NULL;
                }
                ++it;
            }
        }
        cons.clear();
    }
    
    void* data_;
private:
    typename RCluster::pt2RedisConnectFunc connect_;
    typename RCluster::pt2RedisFreeFunc disconnect_;
    RedirectConnections connections_;
    ClusterNodes nodes_;
    std::mutex conLock_;
};

typedef Cluster<redisContext, ThreadedPool<redisContext> > ThreadPoolCluster;

volatile int cnt = 0;
std::mutex lockRedis;

//set global variable value ThreadPoolCluster::ptr_t cluster_p;，set value through timer
ThreadPoolCluster::ptr_t cluster_p;
//std::mutex lockRedis;
void commandThread( ThreadPoolCluster::ptr_t cluster_p )
{
    redisReply * reply;
    // use defined custom cluster as template parameter for HiredisCommand here
    reply = static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "FOO", "SET %d %s", cnt, "BAR1" ) );
    
    // check the result with assert
    assert( reply->type == REDIS_REPLY_STATUS && string(reply->str) == "OK" );
    
    {
        std::lock_guard<std::mutex> locker(lockRedis);
        cout << ++cnt << endl;
    }
    
    freeReplyObject( reply );
}

void processCommandPool()
{
    const int threadsNum = 1000;
    
    // use defined ThreadedPool here
    ThreadPoolCluster::ptr_t cluster_p;
    // and here as template parameter
	cout<<__LINE__<<":"<<redisHost<<endl;
	cout<<__LINE__<<":"<<redisPort<<endl;
    cluster_p = HiredisCommand<ThreadPoolCluster>::createCluster( redisHost.c_str(),boost::lexical_cast<int>(redisPort) );
    
    std::thread thr[threadsNum];
    for( int i = 0; i < threadsNum; ++i )
    {
        thr[i] = std::thread( commandThread, cluster_p );
    }
    
    for( int i = 0; i < threadsNum; ++i )
    {
        thr[i].join();
    }
    
    delete cluster_p;
}

///////////////redis cluster thread pool//////////////////////

void deal_with_flow_number(HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request)
{
     try 
        {
            string temp_flowno="/flowNo/";
            string left_path=request->path.substr(temp_flowno.length(), request->path.length());
            cout<<left_path<<endl;
            std::vector<std::string> one_pair;
            boost::split(one_pair,left_path , boost::is_any_of("/"));


            string company=one_pair[0];
            string type=one_pair[1];
            string id_name="{"+company+"_"+type+"_"+"flow_number}:id";
            string incr_command="incr "+id_name;
            string get_command="get "+id_name;
            cout<<id_name<<endl;
            cout<<incr_command<<endl;
            cout<<get_command<<endl;
            
        //redisReply * incr=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "{flow_number}:id", "incr {flow_number}:id"));
            redisReply * incr=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, id_name.c_str(), incr_command.c_str()));
        freeReplyObject(incr);
        //redisReply * reply=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "{flow_number}:id", "get {flow_number}:id"));
        cout<<__FILE__<<""<<__LINE__<<endl;
        redisReply * reply=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, id_name.c_str(), get_command.c_str()));
        string value="";
        //cout<<__LINE__<<endl;
        if(reply->str!=nullptr)
        {
            //cout<<reply->type<<endl;
          value+=reply->str;
          //retJson.put<std::string>("flow_number",value);
        }
        freeReplyObject(reply);
        cout<<value<<":"<<__FILE__<<""<<__LINE__<<endl;
        ptime now = second_clock::local_time();  
        string now_str  =  to_iso_extended_string(now.date()) + " " + to_simple_string(now.time_of_day());  
        string temp="{\"flowNo\":\""+value+"\",\"replyTime\" : \""+now_str+"\"}";

        cout<<temp<<":"<<__FILE__<<""<<__LINE__<<endl;
        // std::stringstream ss;
        // write_json(ss, retJson);
        // //在这里判断里面的children及childrens的值，如果为空，设置为空数组,用replace
        // string temp=ss.str();
        response <<"HTTP/1.1 200 OK\r\nContent-Length: " << temp.length() << "\r\n\r\n" <<temp;
        }
        catch(json_parser_error& e) 
        {
            string temp="json error";
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << temp.length()<< "\r\n\r\n" << temp;
        }
        catch(exception& e) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
        }
        catch(...) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen("unknown error") << "\r\n\r\n" << "unknown error";
        }
}
void defaultindex(HttpServer& server)
{
	try
	{
		std::lock_guard<std::mutex> locker(lockRedis);
		 server.default_resource["GET"]=[](HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request) {
		string filename="web";
        
		string path=request->path;
        //cout<<path<<endl;
        string temp="/flowNo/";
        if(path.compare(0,temp.length(),temp) == 0)
        {
            deal_with_flow_number(response,request);
            return;
        }
		//Replace all ".." with "." (so we can't leave the web-directory)
		size_t pos;
		while((pos=path.find(".."))!=string::npos) {
			path.erase(pos, 1);
		}
        
		filename+=path;
		ifstream ifs;
		//A simple platform-independent file-or-directory check do not exist, but this works in most of the cases:
		if(filename.find('.')==string::npos) {
			if(filename[filename.length()-1]!='/')
				filename+='/';
			filename+="index.html";
		}
		ifs.open(filename, ifstream::in);
        
		if(ifs) {
			ifs.seekg(0, ios::end);
			size_t length=ifs.tellg();
            
			ifs.seekg(0, ios::beg);

			response << "HTTP/1.1 200 OK\r\nContent-Length: " << length << "\r\n\r\n";
            
			//read and send 128 KB at a time if file-size>buffer_size
			size_t buffer_size=131072;
			if(length>buffer_size) {
				vector<char> buffer(buffer_size);
				size_t read_length;
				while((read_length=ifs.read(&buffer[0], buffer_size).gcount())>0) {
					response.stream.write(&buffer[0], read_length);
					response << HttpServer::flush;
				}
			}
			else
				response << ifs.rdbuf();

			ifs.close();
		}
		else {
			string content="Could not open file "+filename;
			response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << content.length() << "\r\n\r\n" << content;
		}
	};

	}
	catch(exception& e) 
	{
          BOOST_LOG(test_lg::get())<<__LINE__<<": "<<e.what();
	}
}


int apollo(HttpServer& server,string url)
{
	try
	{
         server.resource["^/flowNo/*+$"]["GET"]=[](HttpServer::Response& response, std::shared_ptr<HttpServer::Request> request) {
        try 
        {
            cout<<request->path<<endl;
            ////cout<<__LINE__<<endl;
            string type="";
            string company="";
            string id_name="{"+type+"_"+company+"_"+"flow_number}:id";
            string incr_command="incr "+id_name;
            string get_command="get "+id_name;
            cout<<id_name<<endl;
            cout<<incr_command<<endl;
            cout<<get_command<<endl;
        //redisReply * incr=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "{flow_number}:id", "incr {flow_number}:id"));
            redisReply * incr=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, id_name.c_str(), incr_command.c_str()));
        freeReplyObject(incr);
        //redisReply * reply=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, "{flow_number}:id", "get {flow_number}:id"));
        redisReply * reply=static_cast<redisReply*>( HiredisCommand<ThreadPoolCluster>::Command( cluster_p, id_name.c_str(), get_command.c_str()));
        string value="";
        //cout<<__LINE__<<endl;
        if(reply->str!=nullptr)
        {
            //cout<<reply->type<<endl;
          value+=reply->str;
          //retJson.put<std::string>("flow_number",value);
        }
        freeReplyObject(reply);

        ptime now = second_clock::local_time();  
        string now_str  =  to_iso_extended_string(now.date()) + " " + to_simple_string(now.time_of_day());  
        string temp="{\"flowNo\":"+value+",\"replyTime\" : \""+now_str+"\"}";
        // std::stringstream ss;
        // write_json(ss, retJson);
        // //在这里判断里面的children及childrens的值，如果为空，设置为空数组,用replace
        // string temp=ss.str();
        response <<"HTTP/1.1 200 OK\r\nContent-Length: " << temp.length() << "\r\n\r\n" <<temp;
        }
        catch(json_parser_error& e) 
        {
            string temp="json error";
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << temp.length()<< "\r\n\r\n" << temp;
            return -1;
        }
        catch(exception& e) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen(e.what()) << "\r\n\r\n" << e.what();
            return -1;
        }
        catch(...) {
            response << "HTTP/1.1 400 Bad Request\r\nContent-Length: " << strlen("unknown error") << "\r\n\r\n" << "unknown error";
            return -1;
        }
    };

		return 0;
	}
	catch(exception& e) 
	{
          BOOST_LOG(test_lg::get())<<__LINE__<<": "<<e.what();
		  return -1;
	}
	catch(...) 
	{
          BOOST_LOG(test_lg::get())<<__LINE__<<": "<<"unknown error";
		  return -1;
	}
}

void serverRedisResource(HttpServer& server,string redisHost,string redisPort,string redisPassword,string url)
{
	try
	{
		//init redis connection pool

		 cluster_p = HiredisCommand<ThreadPoolCluster>::createCluster( redisHost.c_str(),boost::lexical_cast<int>(redisPort));
		apollo(server,url);
		defaultindex(server);
	}
	catch(exception& e) 
	{
          BOOST_LOG(test_lg::get())<<__LINE__<<": "<<e.what();
	}
	catch(...) 
	{
         
	}
}







void testhash(Connection &conn)
{
    conn.del("hello");
    cout<<((bool)conn.hset("hello", "world", "one"))<<std::endl;
    cout<<((bool)conn.hset("hello", "mars", "two"))<<std::endl;
    cout<<((std::string)conn.hget("hello", "world") == "one")<<std::endl;
    cout<<(!conn.hsetNX("hello", "mars", "two"))<<std::endl;
    cout<<((bool)conn.hsetNX("hello", "venus", "1"))<<std::endl;
    cout<<(conn.hincrBy("hello", "venus", 3) == 4)<<std::endl;
    cout<<((bool)conn.hexists("hello", "venus"))<<std::endl;
    cout<<((bool)conn.hdel("hello", "venus"))<<std::endl;
    cout<<(!conn.hexists("hello", "venus"))<<std::endl;
    cout<<(conn.hlen("hello") == 2)<<std::endl;
    MultiBulkEnumerator result = conn.hkeys("hello");
    std::string str1;
    std::string str2;
    cout<<(result.next(&str1))<<std::endl;
    cout<<(result.next(&str2))<<std::endl;
    cout<<((str1 == "world" && str2 == "mars") || (str2 == "world" && str1 == "mars"))<<std::endl;
    result = conn.hvals("hello");
    cout<<(result.next(&str1))<<std::endl;
    cout<<(result.next(&str2))<<std::endl;
    cout<<((str1 == "one" && str2 == "two") || (str2 == "one" && str1 == "two"))<<std::endl;
    result = conn.hgetAll("hello");
    std::string str3;
    std::string str4;
    cout<<(result.next(&str1))<<std::endl;
    cout<<(result.next(&str2))<<std::endl;
    cout<<(result.next(&str3))<<std::endl;
    cout<<(result.next(&str4))<<std::endl;
    cout<<(
                    (str1 == "world" && str2 == "one" && str3 == "mars" && str4 == "two")
                    ||
                    (str1 == "mars" && str2 == "two" && str3 == "world" && str4 == "one")
                )<<std::endl;
}

void testset(Connection &conn)
{
	conn.del("hello");
	cout<<((bool)conn.sadd("hello", "world"))<<std::endl;
    cout<<((bool)conn.sisMember("hello", "world"))<<std::endl;
    cout<<(!conn.sisMember("hello", "mars"))<<std::endl;
    cout<<(conn.scard("hello") == 1)<<std::endl;
    cout<<((bool)conn.sadd("hello", "mars"))<<std::endl;
    cout<<(conn.scard("hello") == 2)<<std::endl;
    MultiBulkEnumerator result = conn.smembers("hello");
    std::string str1;
    std::string str2;
    cout<<(result.next(&str1))<<std::endl;
    cout<<(result.next(&str2))<<std::endl;
    cout<<((str1 == "world" && str2 == "mars") || (str2 == "world" && str1 == "mars"))<<std::endl;
    std::string randomMember = conn.srandMember("hello");
    cout<<(randomMember == "world" || randomMember == "mars")<<std::endl;
    cout<<((bool)conn.srem("hello", "mars"))<<std::endl;
    cout<<(conn.scard("hello") == 1)<<std::endl;
    cout<<((std::string)conn.spop("hello") == "world")<<std::endl;
    cout<<(conn.scard("hello") == 0)<<std::endl;
    conn.del("hello1");
    cout<<((bool)conn.sadd("hello", "world"))<<std::endl;
    cout<<(conn.scard("hello") == 1)<<std::endl;
    cout<<((bool)conn.smove("hello", "hello1", "world"))<<std::endl;
    cout<<(conn.scard("hello") == 0)<<std::endl;
    cout<<(conn.scard("hello1") == 1)<<std::endl;
}
void testlist(Connection &conn)
{
	conn.del("hello");
	cout<<(conn.lpush("hello", "c") == 1)<<std::endl;
    cout<<(conn.lpush("hello", "d") == 2)<<std::endl;
    cout<<(conn.rpush("hello", "b") == 3)<<std::endl;
    cout<<(conn.rpush("hello", "a") == 4)<<std::endl;
    cout<<(conn.llen("hello") == 4)<<std::endl;
    MultiBulkEnumerator result = conn.lrange("hello", 1, 3);
    std::string str;
    cout<<(result.next(&str))<<std::endl;
    cout<<(str == "c")<<std::endl;
    cout<<(result.next(&str))<<std::endl;
    cout<<(str == "b")<<std::endl;
    cout<<(result.next(&str))<<std::endl;
    cout<<(str == "a")<<std::endl;
    cout<<conn.ltrim("hello", 0, 1)<<std::endl;
    result = conn.lrange("hello", 0, 10);
    cout<<(result.next(&str))<<std::endl;
    cout<<(str == "d")<<std::endl;
    cout<<(result.next(&str))<<std::endl;
    cout<<(str == "c")<<std::endl;
    cout<<((std::string)conn.lindex("hello", 0) == "d")<<std::endl;
    cout<<((std::string)conn.lindex("hello", 1) == "c")<<std::endl;
    cout<<conn.lset("hello", 1, "f")<<std::endl;
    cout<<((std::string)conn.lindex("hello", 1) == "f")<<std::endl;
    conn.lpush("hello", "f");
    conn.lpush("hello", "f");
    conn.lpush("hello", "f");
	cout<<"................"<<std::endl;
    cout<<(conn.lrem("hello", 2, "f") == 2)<<std::endl;
    cout<<(conn.llen("hello") == 3)<<std::endl;
    cout<<((std::string)conn.lpop("hello") == "f")<<std::endl;
    cout<<(conn.llen("hello") == 2)<<std::endl;
    conn.rpush("hello", "x");
    cout<<((std::string)conn.rpop("hello") == "x")<<std::endl;
    conn.rpush("hello", "z");
    cout<<((std::string)conn.rpopLpush("hello", "hello") == "z")<<std::endl;


}

void run(string host="localhost",string arg="6379",string password="renesola")
{
	try
    {
	cout<<"redis"<<std::endl;
	//cout<<"redis1"<<std::endl;
    Connection conn(host, arg, password);
	conn.select(0);
	conn.set("hello", "world");
    StringReply stringReply = conn.get("hello");
	
    cout<<stringReply.result().is_initialized()<<std::endl;
	
    cout<<(std::string)conn.get("hello")<<std::endl;
	
    cout<<((bool)conn.exists("hello"))<<std::endl;
    cout<<((bool)conn.del("hello"))<<std::endl;
    cout<<(!conn.exists("hello"))<<std::endl;
    cout<<(!conn.del("hello"))<<std::endl;
	cout<<(conn.type("hello") == String)<<std::endl;
	
	cout<<"///////////////////////////////////////"<<std::endl;

	testlist(conn);
	cout<<"///////////////////////////////////////"<<std::endl;
	testset(conn);
	cout<<"///////////////////////////////////////"<<std::endl;
	testhash(conn);
	cout<<"///////////////////////////////////////"<<std::endl;
	/*testzset(conn);*/
	 }
    catch (std::exception& e)
    {
        std::cout << "FAILURE: " << e.what() << std::endl;
       // cout <<GetStackTrace()<< std::endl;
    }

}


std::string&   replace_all(std::string&   str,const   std::string&   old_value,const   std::string&   new_value)     
{     
    while(true)   {     
        std::string::size_type   pos(0);     
        if(   (pos=str.find(old_value))!=std::string::npos   )     
            str.replace(pos,old_value.length(),new_value);     
        else   break;     
    }     
    return   str;     
}     
std::string&   replace_all_distinct(std::string&   str,const   std::string&   old_value,const   std::string&   new_value)     
{     
    for(std::string::size_type   pos(0);   pos!=std::string::npos;   pos+=new_value.length())   {     
        if(   (pos=str.find(old_value,pos))!=std::string::npos   )     
            str.replace(pos,old_value.length(),new_value);     
        else   break;     
    }     
    return   str;     
} 
#endif	