#include "main.h"


int main(void) {

	curl_global_init(CURL_GLOBAL_ALL);


	cout << "OK" << endl ;

	TPlatform p ;


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

   curl_global_cleanup();
}

/*
\rm testing ; make testing ; ./testing
*/