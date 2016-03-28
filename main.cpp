#include "main.h"
#include "mongoose.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>

#define CONFIG_FILE "config.json"

std::mutex g_log_mutex;

void add2log ( string s ) {
	std::lock_guard<std::mutex> lock(g_log_mutex);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    
    char file[200] , timestamp[200] ;
    strftime ( file , sizeof(file) , "./%Y-%m.log" , &tm ) ;
    strftime ( timestamp , sizeof(timestamp) , "%d-%m-%Y %H-%M-%S" , &tm ) ;
    
    ofstream outfile;
    outfile.open(file, std::ios_base::app);
    outfile << timestamp << "\t" << s << endl ;
    outfile.close() ;
}

std::mutex g_main_mutex;

string mg_str2string ( const mg_str &s ) {
	string ret ;
	const char *p = s.p ;
	ret.reserve ( s.len ) ;
	for ( size_t pos = 0 ; pos < s.len ; pos++ , p++ ) ret += (char) *p ;
	return ret ;
}

static void ev_handler(struct mg_connection *c, int ev, void *p) {
	if (ev != MG_EV_HTTP_REQUEST)  return ;

	c->flags |= MG_F_SEND_AND_CLOSE;
	struct http_message *hm = (struct http_message *) p;

	string out , path , query ;
	string type = "application/json" ;

	if ( 1 ) {
		std::lock_guard<std::mutex> lock(g_main_mutex);
		string method = mg_str2string ( hm->method ) ;
		path = mg_str2string ( hm->uri ) ;
		if ( method == "GET" ) {
			query = mg_str2string ( hm->query_string ) ;
		} else if ( method == "POST" ) {
			query = mg_str2string(hm->body) ;
		} else {
			if(DEBUG_OUTPUT) cout << "Unknown method " << method << endl ;
			return ;
		}
	}

	if ( path == "/" ) {
		add2log ( "query\t" + query ) ;
		if(DEBUG_OUTPUT) cout << path << " | " << query << endl ;
	}

	if ( path == "/" && !query.empty() ) {

//		if(DEBUG_OUTPUT) cout << "Running query!\n" ;
	
		TPlatform platform ;
		platform.setConfig ( *root_platform ) ;
		platform.query = query ;
	
		// Parse query into map
		map <string,string> params ;
		vector <string> parts ;
		split ( query , parts , '&' ) ;
		for ( auto i = parts.begin() ; i != parts.end() ; i++ ) {
			if ( i->empty() ) continue ;
			vector <string> tmp ;
			split ( *i , tmp , '=' , 2 ) ;
			if ( tmp.size() == 1 ) tmp.push_back ( "" ) ;
			tmp[0] = urldecode ( tmp[0] ) ;
			tmp[1] = urldecode ( tmp[1] ) ;
//			if(DEBUG_OUTPUT) cout << "  " << tmp[0] << " = " << tmp[1] << endl ;
			platform.params[tmp[0]] = tmp[1] ;
		}

		type = "text/html" ;
		string filename = "html/index.html" ;
		char *buffer = loadFileFromDisk ( filename ) ;
		out = string ( buffer ) ;
		free ( buffer ) ;
	
	
		if ( platform.params.find("doit") != platform.params.end() ) {
			string results = platform.process() ;

			type = platform.content_type ;

			// Replace in HTML output
			if ( platform.params["format"] == "html" ) {
				string key ;
				size_t pos ;

				key = "<!--querystring-->" ;
				pos = out.find(key) ;
				out = out.substr(0,pos) + urlencode(query) + out.substr(pos+key.length()) ;

			
				key = "<!--output-->" ;
				pos = out.find(key) ;
				out = out.substr(0,pos) + results + out.substr(pos+key.length()) ;

			} else {

				out.swap ( results ) ;
			}
		}

	
	} else if ( path == "/" || path.substr(path.length()-3,3)==".js" || path.substr(path.length()-4,4)==".css" || path.substr(path.length()-4,4)==".map" ) {

		string filename = "html" + path ;
		if ( path == "/" ) filename = "html/index.html" ;

		type = "text/html" ;
		if ( path.length() > 5 ) {
			if ( path.substr(path.length()-3,3)==".js" ) type = "application/javascript" ;
			if ( path.substr(path.length()-4,4)==".css" ) type = "text/css" ;
			if ( path.substr(path.length()-4,4)==".map" ) type = "text/plain" ;
		}

		// There is some issue here that causes the server to die. Caching seems to prevent this.
		out = loadAndCacheFileFromDisk ( filename ) ;

	} else {
		char reply[1000];

		// Send the reply
		snprintf(reply, sizeof(reply), "{ \"uri\": \"%.*s\" }\n",
				 (int) hm->uri.len, hm->uri.p);
		 
		out = reply ;
	}

	mg_printf(c, "HTTP/1.1 200 OK\r\n"
			  "Content-Type: %s\r\n"
			  "Content-Length: %d\r\n"
			  "\r\n"
			  "%s",
			  type.c_str() ,
			  (int) out.length(), out.c_str());

}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;

	root_platform = new TPlatform ;
	if ( !root_platform->readConfigFile ( CONFIG_FILE ) ) exit ( 1 ) ;
	int timeout = atoi(root_platform->config["timeout"].c_str()) ;

	mg_mgr_init(&mgr, NULL);
	nc = mg_bind(&mgr, root_platform->config["port"].c_str() , ev_handler); // s_http_port
	mg_set_protocol_http_websocket(nc);

	/* For each new connection, execute ev_handler in a separate thread */
	mg_enable_multithreading(nc);

	cout << "STARTING SERVER\tport " << root_platform->config["port"] << endl ;
	add2log ( "STARTING" ) ;
	for (;;) {
		mg_mgr_poll(&mgr, timeout );
	}
	mg_mgr_free(&mgr);

	return 0;
}