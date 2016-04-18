#include "main.h"





int main(void) {
	curl_global_init(CURL_GLOBAL_ALL);
	
	TPageList pagelist ( "enwiki" ) ;
	pagelist.pages.push_back ( TPage ( "Richard Dawkins" , 0 ) ) ;
	pagelist.pages.push_back ( TPage ( "Community portal" , 4 ) ) ;
	
	
	
	string url = "https://tools.wmflabs.org/pagepile/api.php" ;
	string params = "action=create_pile_with_data&wiki="+pagelist.wiki+"&data=" ;
	for ( auto page:pagelist.pages ) {
		params += urlencode(page.name) + "%09" + ui2s(page.meta.ns) + "%0A" ;
	}
	

	json j ;
	bool b = loadJSONfromPOST ( url , params , j ) ;
	
	if ( !b ) {
		cout << "FAIL!\n" ;
		exit ( 1 ) ;
	}
	
	int pid = j["pile"]["id"] ;
	cout << "PID: " << pid << endl ;

/*	
//	cout << params << endl ; return 0;
	
	auto curl = curl_easy_init();
	if ( !curl ) {
		cout << "No CURL\n" ;
		exit ( 1 ) ;
	}
	
	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, "name=daniel&project=curl");
	auto res = curl_easy_perform(curl);
	if(res != CURLE_OK) {
		fprintf(stderr, "curl_easy_perform() failed: %s\n", curl_easy_strerror(res));
	}



	if ( chunk.size == 0 || !chunk.memory ) return false ;
	
	char *text = chunk.memory ;
	curl_easy_cleanup(curl);



    curl_easy_cleanup(curl);
*/

/*
	cout << "Running..." << endl ;

	TPlatform p ;
	root_platform = &p ;
	if ( !p.readConfigFile ( "config.json" ) ) exit ( 1 ) ;

	TSourceSPARQL sparql ( &p ) ;
//	if ( sparql.runQuery ( "SELECT ?item WHERE { ?item wdt:P31 wd:Q146 }" ) ) {
	if ( sparql.runQuery ( "SELECT ?item WHERE { ?item wdt:P106 wd:Q937857 . ?item wdt:P27 wd:Q29 }" ) ) {
		cout << "Read " << sparql.size() << " items\n" ;
		for ( int a = 0 ; a < 5 ; a++ ) cout << sparql.pages[a].name << endl ;
	}

	TSourcePagePile pp ( &p ) ;
	if ( pp.getPile ( 2567 ) ) {
		cout << "Read " << pp.size() << " items\n" ;
		for ( int a = 0 ; a < 5 ; a++ ) cout << pp.pages[a].name << endl ;
	}

	sparql.intersect ( pp ) ;
	
	cout << "Now " << sparql.size() << " items\n" ;

	if ( 0 ) {
		TSourceDatabase db ( &p ) ;
		TSourceDatabaseParams params ;
		params.wiki = "dewiki" ;
		params.default_depth = 2 ;
		params.page_namespace_ids.push_back ( 1 ) ;
		params.positive.push_back ( TSourceDatabaseCatDepth ( "Wikipedia:Archiv mit relevanter Versionsgeschichte" , 0 ) ) ;

		if ( db.getPages ( params ) ) {
			cout << "Read " << db.size() << " items\n" ;
			for ( int a = 0 ; a < 5 ; a++ ) cout << db.pages[a].name << endl ;
		} else {
			cout << "Nope" << endl ;
		}
	}
*/
   curl_global_cleanup();
}

/*
\rm testing ; make testing ; ./testing
*/