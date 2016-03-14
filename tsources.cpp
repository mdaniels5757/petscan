#include "main.h"

void TPageList::sort () {
	if ( is_sorted ) return ;
	std::sort ( pages.begin() , pages.end() ) ;
}

void TPageList::intersect ( TPageList &pl ) {
	if ( wiki != pl.wiki ) {
		error ( "Intersecting " + wiki + " and " + pl.wiki + " not possible" ) ;
		return ;
	}
	sort() ;
	pl.sort() ;
	vector <TPage> nl ;
	auto first1 = pages.begin() ;
	auto last1 = pages.end() ;
	auto first2 = pl.pages.begin() ;
	auto last2 = pl.pages.end() ;
	while (first1!=last1 && first2!=last2) {
		if (*first1<*first2) ++first1;
		else if (*first2<*first1) ++first2;
		else {
			nl.push_back ( *first1 ) ;
			++first1; ++first2;
		}
	}
	pages = nl ;
}

void TPageList::merge ( TPageList &pl ) {
	if ( wiki != pl.wiki ) {
		error ( "Merging of " + wiki + " and " + pl.wiki + " not possible" ) ;
		return ;
	}
	sort() ;
	pl.sort() ;
	vector <TPage> nl ;
	auto first1 = pages.begin() ;
	auto last1 = pages.end() ;
	auto first2 = pl.pages.begin() ;
	auto last2 = pl.pages.end() ;
	while (first1!=last1 && first2!=last2) {
		if (*first1<*first2) {
			nl.push_back ( *first1 ) ;
			++first1;
		} else if (*first2<*first1) {
			nl.push_back ( *first2 ) ;
			++first2;
		} else {
			nl.push_back ( *first1 ) ;
			++first1; ++first2;
		}
	}
	while ( first1!=last1 ) nl.push_back ( *first1++ ) ;
	while ( first2!=last2 ) nl.push_back ( *first2++ ) ;
	pages = nl ;
}


bool TSource::error ( string s ) {
	if ( platform ) platform->error ( s ) ;
	return false ;
}


bool TSourceSPARQL::runQuery ( string query ) {
	clear() ;
	wiki = "wikidatawiki" ;
	

// TODO BEGIN FUNCTION
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return error ( "Cannot init curl" ) ;

	string real_query = sparql_prefixes + query ;
	char *encoded_query = curl_easy_escape ( curl , real_query.c_str() , 0 ) ;
	string url = string("https://query.wikidata.org/sparql?format=json&query=") + encoded_query ;
	curl_free(encoded_query) ;

	struct CURLMemoryStruct chunk;
	chunk.memory = (char*) malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect; paranoia
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "wdq-agent/1.0"); // fake agent
	
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		string e = string("curl_easy_perform() failed for ") + url + string(": ") + curl_easy_strerror(res) ;
		return error ( e ) ;
	}
	
	if ( chunk.size == 0 || !chunk.memory ) return error ( "CURL chunk: No memory" ) ;
	
	
	char *text = chunk.memory ;
//	if(chunk.memory) free(chunk.memory);
	curl_easy_cleanup(curl);

//	cout << text << endl ;

	if ( *text != '{' ) {
		free ( text ) ;
		return error ( "SPARQL return does not start with '{'" ) ;
	}
	
	MyJSON j ( text ) ;
	free ( text ) ;
// TODO END FUNCTION

	string item_key = j["head"]["vars"][0].s ;
	pages.reserve ( j["results"]["bindings"].a.size() ) ;
	for ( uint32_t i = 0 ; i < j["results"]["bindings"].a.size() ; i++ ) {
		string v = j["results"]["bindings"][i][item_key]["value"].s ;
		const char *last , *c ;
		for ( last = NULL , c = v.c_str() ; *c ; c++ ) {
			if ( *c == '/' ) last = c+1 ;
		}
		if ( !last ) continue ;
		pages.push_back ( TPage ( last , 0 ) ) ;
	}

	return true ;
}


bool TSourcePagePile::getPile ( uint32_t id ) {
	char s[200] ;
	sprintf ( s , "https://tools.wmflabs.org/pagepile/api.php?id=%d&action=get_data&format=json&doit" , id ) ;
	string url = s ;

// TODO BEGIN FUNCTION
	CURL *curl;
	curl = curl_easy_init();
	if ( !curl ) return error ( "Cannot init curl" ) ;

	struct CURLMemoryStruct chunk;
	chunk.memory = (char*) malloc(1);  /* will be grown as needed by the realloc above */ 
	chunk.size = 0;    /* no data at this point */ 

	curl_easy_setopt(curl, CURLOPT_URL, url.c_str());
	curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L); // Follow redirect; paranoia
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteMemoryCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, (void *)&chunk);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "wdq-agent/1.0"); // fake agent
	
	CURLcode res = curl_easy_perform(curl);
	if (res != CURLE_OK) {
		string e = string("curl_easy_perform() failed for ") + url + string(": ") + curl_easy_strerror(res) ;
		return error ( e ) ;
	}
	
	if ( chunk.size == 0 || !chunk.memory ) return error ( "CURL chunk: No memory" ) ;
	
	
	char *text = chunk.memory ;
//	if(chunk.memory) free(chunk.memory);
	curl_easy_cleanup(curl);

//	cout << text << endl ;

	if ( *text != '{' ) {
		free ( text ) ;
		return error ( "SPARQL return does not start with '{'" ) ;
	}
	
	MyJSON j ( text ) ;
	free ( text ) ;
// TODO END FUNCTION
	
	
	clear() ;
	wiki = j["wiki"].s ;
	for ( uint32_t i = 0 ; i < j["pages"].size() ; i++ ) pages.push_back ( j["pages"][i].s ) ;
	return true ;
}


bool TSourceDatabase::getPages ( TSourceDatabaseParams &params ) {
}
