#include "main.h"


int main(void) {

	curl_global_init(CURL_GLOBAL_ALL);

/*
	// JSON string escape testing
	setlocale(LC_ALL, "");
    cout << "LC_ALL: " << setlocale(LC_ALL, NULL) << endl;
    cout << "LC_CTYPE: " << setlocale(LC_CTYPE, NULL) << endl;	

	MyJSON j ;
	string s = "ბერლინი" ;
	string t = j.escapeString ( s ) ;
	string u = j.unescapeString ( t ) ;
	cout << s << endl ;
	cout << t << endl ;
	cout << u << endl ;
	exit ( 0 ) ;
*/

	cout << "Running..." << endl ;

	TPlatform p ;
	if ( !p.readConfigFile ( "config.json" ) ) exit ( 1 ) ;

/*
	TSourceSPARQL sparql ( &p ) ;
//	if ( sparql.runQuery ( "SELECT ?item WHERE { ?item wdt:P31 wd:Q146 }" ) ) {
	if ( sparql.runQuery ( "SELECT ?item WHERE { ?item wdt:P106 wd:Q937857 . ?item wdt:P27 wd:Q29 }" ) ) {
		cout << "Read " << sparql.size() << " items\n" ;
	}


	TSourcePagePile pp ( &p ) ;
	if ( pp.getPile ( 2567 ) ) {
		cout << "Read " << pp.size() << " items\n" ;
	}

	sparql.merge ( pp ) ;
	cout << "Now " << sparql.size() << " items\n" ;
*/

	TSourceDatabase db ( &p ) ;
	TSourceDatabaseParams params ;
	params.wiki = "dewiki" ;
	params.page_namespace_ids.push_back ( 0 ) ;
//	params.positive.push_back ( TSourceDatabaseCatDepth ( "Mann" , 0 ) ) ;
	params.positive.push_back ( TSourceDatabaseCatDepth ( "Hochschullehrer (Ludwig-Maximilians-Universität München)" , 0 ) ) ;

	if ( db.getPages ( params ) ) {
		cout << "Read " << db.size() << " items\n" ;
	} else {
		cout << "Nope" << endl ;
	}

   curl_global_cleanup();
}

/*
\rm testing ; make testing ; ./testing
*/