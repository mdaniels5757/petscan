#include "main.h"
#include "mongoose.h"
#include <iostream>
#include <fstream>
#include <iomanip>
#include <ctime>
#include <unistd.h>

#define CONFIG_FILE "config.json"
#define MAX_QUERY_LENGTH 4096

bool site_is_locked = false ;
int32_t running = 0 ;
std::mutex g_main_mutex;
TWikidataDB mysql_logging ;
std::mutex g_log_mutex , g_log_db_mutex ;

uint32_t logQuery ( string query ) {
	if ( query.empty() ) return 0 ;
	if ( query.length() > MAX_QUERY_LENGTH ) return 0 ;
	std::lock_guard<std::mutex> lock(g_log_db_mutex);
	
	uint32_t ret = 0 ;
	string sql ;
	
	// Get existing
	sql = "SELECT id FROM query WHERE querystring='" + mysql_logging.escape(query) + "' LIMIT 1" ;
	MYSQL_RES *result = mysql_logging.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) ret = atol ( row[0] ) ;
	mysql_free_result(result);
	if ( ret != 0 ) return ret ; // Found existing query
	
	// Create new
    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    char timestamp[200] ;
    strftime ( timestamp , sizeof(timestamp) , "%d-%m-%Y %H:%M:%S" , &tm ) ;
	sql = "INSERT IGNORE INTO query ( querystring , created ) VALUES ( '" + mysql_logging.escape(query) + "','" + string ( timestamp ) + "')" ;
	mysql_logging.runQuery ( sql ) ;
	ret = mysql_logging.lastInsertID() ; // Stored as new query

	return ret ;
}

string getQueryFromPSID ( uint32_t psid ) {
	string ret  ;
	if ( psid == 0 ) return ret ;

	std::lock_guard<std::mutex> lock(g_log_db_mutex);
	char sql[1000] ;
	sprintf ( sql , "SELECT querystring FROM query WHERE id=%d" , psid ) ;
	MYSQL_RES *result = mysql_logging.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) ret = row[0] ;
	mysql_free_result(result);
	return ret ;
}



void add2log ( string s ) {
	std::lock_guard<std::mutex> lock(g_log_mutex);

    auto t = std::time(nullptr);
    auto tm = *std::localtime(&t);
    
    char file[200] , timestamp[200] ;
    strftime ( file , sizeof(file) , "./%Y-%m.log" , &tm ) ;
    strftime ( timestamp , sizeof(timestamp) , "%d-%m-%Y %H:%M:%S" , &tm ) ;
    
    ofstream outfile;
    outfile.open(file, std::ios_base::app);
    outfile << timestamp << "\t" << s << endl ;
    outfile.close() ;
}

string mg_str2string ( const mg_str &s ) {
	string ret ;
	const char *p = s.p ;
	ret.reserve ( s.len ) ;
	for ( size_t pos = 0 ; pos < s.len ; pos++ , p++ ) ret += (char) *p ;
	return ret ;
}

void parseQueryParameters ( string &query , map <string,string> &params ) {
	params.clear() ;
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
		params[tmp[0]] = tmp[1] ;
	}
}


static void ev_handler(struct mg_connection *c, int ev, void *p) {
	if ( site_is_locked ) return ;
	if (ev != MG_EV_HTTP_REQUEST)  return ;

	running++ ;
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
			stringReplace ( query , "+" , "%20" ) ;
		} else if ( method == "POST" ) {
			query = mg_str2string(hm->body) ;
			stringReplace ( query , "+" , "%20" ) ;
		} else {
			if(DEBUG_OUTPUT) cout << "Unknown method " << method << endl ;
			running-- ;
			return ;
		}
	}
	
	if ( path == "/restart" && query == root_platform->config["restart-code"] ) {
		site_is_locked = true ;
		uint32_t cnt = 0 ;
		while ( running > 1 && cnt < 40 ) { // 20 sec max
			usleep ( 500000 ) ;
			cnt++ ;
		}
		cout << "restarting after " << int(cnt/2) << " seconds..." << endl ;
		exit ( 0 ) ;
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
		parseQueryParameters ( query , platform.params ) ;
		
		// Based on existing PSID?
		bool based_on_psid = false ;
		bool has_psid_additional_parameters = false ;
		if ( platform.params.find("psid") != platform.params.end() ) {
			map <string,string> p2 ;
			platform.psid = atol(platform.params["psid"].c_str()) ;
			string q2 = getQueryFromPSID ( platform.psid ) ;
			if ( !q2.empty() ) {
				based_on_psid = true ;
				parseQueryParameters ( q2 , p2 ) ;
				for ( auto i = platform.params.begin() ; i != platform.params.end() ; i++ ) {
					if ( i->first == "psid" ) continue ;
					p2[i->first] = i->second ;
					has_psid_additional_parameters = true ;
				}
				platform.params.swap ( p2 ) ;
				
				// Replace current query with merged one
				query.clear() ;
				for ( auto i = platform.params.begin() ; i != platform.params.end() ; i++ ) {
					if ( i->first == "psid" ) continue ; // PARANOIA: Avoid recursive queries, as these could DDOS the tool
					if ( !query.empty() ) query += "&" ;
					query += urlencode(i->first) + "=" + urlencode(i->second) ;
				}
			}
		}

		if ( !based_on_psid || has_psid_additional_parameters ) {
			platform.psid = logQuery ( query ) ;
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

				if ( platform.psid ) {
					char tmp[200] ;
					sprintf ( tmp , "<span name='psid' style='display:none'>%d</span>" , platform.psid ) ;
					key = "<!--psid-->" ;
					pos = out.find(key) ;
					out = out.substr(0,pos) + tmp + out.substr(pos+key.length()) ;
				}

			} else {

				out.swap ( results ) ;
			}
		}

	
	} else if ( path == "/" || path.substr(path.length()-3,3)==".js" || path.substr(path.length()-4,4)==".css" || path.substr(path.length()-4,4)==".map" || path.substr(path.length()-4,4)==".txt" ) {

		string filename = "html" + path ;
		if ( path == "/" ) filename = "html/index.html" ;

		type = "text/html" ;
		if ( path.length() > 5 ) {
			if ( path.substr(path.length()-3,3)==".js" ) type = "application/javascript" ;
			if ( path.substr(path.length()-4,4)==".css" ) type = "text/css" ;
			if ( path.substr(path.length()-4,4)==".map" ) type = "text/plain" ;
			if ( path.substr(path.length()-4,4)==".txt" ) type = "text/plain" ;
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

	running-- ;
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;
	
	srand (time(NULL));

	root_platform = new TPlatform ;
	if ( !root_platform->readConfigFile ( CONFIG_FILE ) ) exit ( 1 ) ;
	int timeout = atoi(root_platform->config["timeout"].c_str()) ;

	// Set up query-logging DB
	mysql_logging.setHostDB ( "tools.labsdb" , "s51434__petscan" ) ;
	mysql_logging.doConnect ( true ) ;


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