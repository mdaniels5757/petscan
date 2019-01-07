#include "main.h"

TPlatform *root_platform ;
std::mutex g_root_platform_mutex;

//________________________________________________________________________________________________

#define DB_ESCAPE_BUFFER_SIZE 1000000


TWikidataDB::TWikidataDB ( string wiki , TPlatform *_platform ) {
	platform = _platform ;
	setHostDBFromWiki ( wiki ) ;
	curl_global_init(CURL_GLOBAL_ALL);
	doConnect(true) ;
}	

bool TWikidataDB::setHostDBFromWiki ( string wiki ) {
	_wiki = wiki ;
	vector <string> parts = { "quote" , "source" , "books" , "source" , "news" , "versity" , "voyage" } ;
	for ( auto p: parts ) {
		stringReplace ( wiki , "wiki"+p+"wiki" , "wiki"+p ) ;
	}
	stringReplace ( wiki , "-" , "_" ) ;
	stringReplace ( wiki , "wikispecieswiki" , "specieswiki" ) ;
	stringReplace ( wiki , "specieswikimedia" , "specieswiki" ) ;
	stringReplace ( wiki , "wiktionarywiki" , "wiktionary" ) ;
	stringReplace ( wiki , "be_taraskwiki" , "be_x_oldwiki" ) ;

	if ( 0 ) { // Future special cases
	} else {
//		_host = wiki+".web.db.svc.eqiad.wmflabs" ; // Fast ones
		_host = wiki+".analytics.db.svc.eqiad.wmflabs" ; // New, long-query server, use this if possible
//		_host = wiki+".labsdb" ; // Old host, try not to use
		_database = wiki+"_p" ;
	}
}

bool TWikidataDB::isConnected() {
	return did_connect ;
}

void TWikidataDB::doConnect ( bool first ) {
	if ( !first ) {
		int i = mysql_ping ( &mysql ) ;
		if ( i == 0 ) return ; // Connection is already up

		if ( string("MySQL server has gone away") == string(mysql_error(&mysql)) ) return doConnect(true) ;

//		if ( i != CR_SERVER_GONE_ERROR ) {
			finishWithError() ;
//			cerr << "MySQL PING says: " << i << endl ;
//			exit ( 0 ) ;
//		}
	}
	
	std::lock_guard<std::mutex> guard(g_root_platform_mutex);
	

	if ( root_platform == NULL ) {
		cout << "Root platform not set\n" ;
		exit ( 1 ) ;
	}

	string user = root_platform->config["user"] ;
	string password = root_platform->config["password"] ;
	mysql_init(&mysql);
	if ( _force_utf8 ) {
		mysql_options(&mysql, MYSQL_SET_CHARSET_NAME, "utf8"); 
		mysql_options(&mysql, MYSQL_INIT_COMMAND, "SET NAMES utf8"); 
	}
	if ( mysql_real_connect(&mysql,_host.c_str(),user.c_str(),password.c_str(),_database.c_str(),0,NULL,0) ) {
		did_connect = true ;
	} else {
//		string error = mysql_error(&mysql) ;
//		if ( error.find("Unknown MySQL server host ") == 0 ) 
		did_connect = false ;
		finishWithError() ;
	}
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
		return "" ;
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
	if ( mysql_query ( &mysql , sql.c_str() ) ) {
		if ( string("MySQL server has gone away") == string(mysql_error(&mysql)) ) {
			doConnect(true) ;
			return runQuery ( sql ) ;
		}
		finishWithError() ;
	}
}

MYSQL_RES *TWikidataDB::getQueryResults ( string sql ) {
	runQuery ( sql ) ;
	MYSQL_RES *result = mysql_store_result(&mysql);
	if ( result == NULL ) {
		if ( string("MySQL server has gone away") == string(mysql_error(&mysql)) ) {
			doConnect(true) ;
			return getQueryResults ( sql ) ;
		}
		fprintf(stderr, "!! %s\n", mysql_error(&mysql));
		finishWithError("SQL problem:"+sql) ;
	}
	return result ;
}

void TWikidataDB::finishWithError ( string msg ) {
//	if ( platform ) platform->error ( _wiki+": "+msg+"\n"+string(mysql_error(&mysql)) ) ;
	if ( msg.empty() ) fprintf(stderr, "%s\n", mysql_error(&mysql));
	else fprintf(stderr, "%s:%s\n", _wiki.c_str(),msg.c_str());
	mysql_close(&mysql);
}

void TWikidataDB::setHostDB ( string host , string db , bool force_utf8 ) {
	_host = host ;
	_database = db ;
	_force_utf8 = force_utf8 ;
}

uint32_t TWikidataDB::lastInsertID () {
	return mysql_insert_id ( &mysql ) ;
}
