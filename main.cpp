#include "main.h"
#include "mongoose.h"



//________________________________________________________________________________________________

//static const char *s_http_port = "8083";
//int timeout_ms = 3000 ;

static void ev_handler(struct mg_connection *c, int ev, void *p) {
  if (ev == MG_EV_HTTP_REQUEST) {
    struct http_message *hm = (struct http_message *) p;

    string out ;
    string type = "application/json" ;

	string path ( hm->uri.p ) ;
	path = path.substr ( 0 , hm->uri.len ) ;
	cout << "!!: " << path << endl ;
	if ( path == "/" || path == "/index.html" || path.substr(path.length()-3,3)==".js" || path.substr(path.length()-4,4)==".css" || path.substr(path.length()-4,4)==".map" ) {
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
	if ( !p.readConfigFile ( "config.json" ) ) exit ( 1 ) ;

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