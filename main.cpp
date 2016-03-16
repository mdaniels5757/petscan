#include "main.h"
#include "mongoose.h"

#define CONFIG_FILE "config.json"

//________________________________________________________________________________________________

//static const char *s_http_port = "8083";
//int timeout_ms = 3000 ;

string mg_str2string ( const mg_str &s ) {
	string ret ;
	const char *p = s.p ;
	ret.reserve ( s.len ) ;
	for ( size_t pos = 0 ; pos < s.len ; pos++ , p++ ) ret += (char) *p ;
	return ret ;
}

static void ev_handler(struct mg_connection *c, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    struct http_message *hm = (struct http_message *) p;

    string out ;
    string type = "application/json" ;

	string path = mg_str2string ( hm->uri ) ;
	string query = mg_str2string ( hm->query_string ) ;
	cout << "!!: " << path << " with " << query << endl ;
	
	if ( path == "/" && !query.empty() ) {
	
		cout << "Running query!\n" ;
		
		TPlatform platform ;
		if ( !platform.readConfigFile ( CONFIG_FILE ) ) return ; // error
		
		// Parse query into map
		map <string,string> params ;
		vector <string> parts ;
		split ( query , parts , '&' ) ;
		for ( auto i = parts.begin() ; i != parts.end() ; i++ ) {
			vector <string> tmp ;
			split ( *i , tmp , '=' , 2 ) ;
			if ( tmp.size() == 1 ) tmp.push_back ( "" ) ;
			tmp[0] = urldecode ( tmp[0] ) ;
			tmp[1] = urldecode ( tmp[1] ) ;
			cout << "  " << tmp[0] << " = " << tmp[1] << endl ;
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
			if ( type == "text/html" ) {
				string key = "<!--output-->" ;
				size_t pos = out.find(key) ;
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
		
		char *buffer = loadFileFromDisk ( filename ) ;
		out = string ( buffer ) ;
		free ( buffer ) ;
	} else {
		char reply[1000];

		/* Simulate long calculation */
	//    sleep(3);

		cout << hm->uri.p << endl ;

		/* Send the reply */
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
}

int main(void) {
	struct mg_mgr mgr;
	struct mg_connection *nc;

	TPlatform p ;
	if ( !p.readConfigFile ( CONFIG_FILE ) ) exit ( 1 ) ;

//	TWikidataDB db ( "petscan.conf" , "enwiki.labsddb" ) ;

	mg_mgr_init(&mgr, NULL);
	nc = mg_bind(&mgr, p.config["port"].c_str() , ev_handler); // s_http_port
	mg_set_protocol_http_websocket(nc);

	/* For each new connection, execute ev_handler in a separate thread */
	mg_enable_multithreading(nc);

	printf("Starting multi-threaded server on port %s\n", p.config["port"].c_str() ); // s_http_port
	for (;;) {
		mg_mgr_poll(&mgr, atoi(p.config["timeout"].c_str()) );
	}
	mg_mgr_free(&mgr);

	return 0;
}