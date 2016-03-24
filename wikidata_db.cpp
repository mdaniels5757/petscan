#include "main.h"

TPlatform *root_platform ;

//________________________________________________________________________________________________

#define DB_ESCAPE_BUFFER_SIZE 10000

TWikidataDB::TWikidataDB ( string wiki ) {
	setHostDBFromWiki ( wiki ) ;
	curl_global_init(CURL_GLOBAL_ALL);
	doConnect(true) ;
}	

bool TWikidataDB::setHostDBFromWiki ( string wiki ) {
	if ( 0 ) { // Future special cases
	} else {
		_host = wiki+".labsdb" ;
		_database = wiki+"_p" ;
	}
}

void TWikidataDB::doConnect ( bool first ) {	
	if ( !first ) {
		int i = mysql_ping ( &mysql ) ;
		if ( i == 0 ) return ; // Connection is already up
/*		if ( i != CR_SERVER_GONE_ERROR ) {
			cerr << "MySQL PING says: " << i << endl ;
			exit ( 0 ) ;
		}*/
	}

	if ( root_platform == NULL ) {
		cout << "Root platform not set\n" ;
		exit ( 1 ) ;
	}

	string user = root_platform->config["user"] ;
	string password = root_platform->config["password"] ;
	mysql_init(&mysql);
	mysql_real_connect(&mysql,_host.c_str(),user.c_str(),password.c_str(),_database.c_str(),0,NULL,0) ;
//	printf("MySQL Server Version is %s\n",mysql_get_server_info(&mysql));
}

TWikidataDB::~TWikidataDB () {
   mysql_close(&mysql);
   curl_global_cleanup();
}

string TWikidataDB::escape ( string s ) {
	unsigned long len = s.length() * 2 + 1 ;
	if ( len >= DB_ESCAPE_BUFFER_SIZE ) {
		cerr << "DB:Escape for\n" << s << "\nrequires " << len << " bytes, but limit is " << DB_ESCAPE_BUFFER_SIZE << endl ;
		exit ( 1 ) ;
	}
	char tmp[DB_ESCAPE_BUFFER_SIZE] ; // Would prefer "new" below, but that dies for some reason on "delete"
//	char *tmp = new char ( len ) ;
	long unsigned int end = mysql_real_escape_string ( &mysql , tmp , s.c_str() , s.length() ) ;
	string ret ( tmp ) ;
//	delete [] tmp ;
	return ret ;
}

void TWikidataDB::runQuery ( string sql ) {
	doConnect() ;
	if ( mysql_query ( &mysql , sql.c_str() ) ) finishWithError() ;
}

MYSQL_RES *TWikidataDB::getQueryResults ( string sql ) {
	runQuery ( sql ) ;
	MYSQL_RES *result = mysql_store_result(&mysql);
	if ( result == NULL ) finishWithError(sql+": No result") ;
	return result ;
}

void TWikidataDB::finishWithError ( string msg ) {
	if ( msg.empty() ) fprintf(stderr, "%s\n", mysql_error(&mysql));
	else fprintf(stderr, "%s\n", msg.c_str());
	mysql_close(&mysql);
}
