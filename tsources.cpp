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

//________________________________________________________________________________________________________________________


bool TSource::error ( string s ) {
	if ( platform ) platform->error ( s ) ;
	return false ;
}


//________________________________________________________________________________________________________________________



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

//________________________________________________________________________________________________________________________


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

//________________________________________________________________________________________________________________________


string TSourceDatabase::listEscapedStrings ( TWikidataDB &db , vector <string> &s ) {
	string ret ;
	cout << "Merging " << s.size() << " entries...\n" ;
	for ( auto i = s.begin() ; i != s.end() ; i++ ) {
		if ( i != s.begin() ) ret += "," ;
		ret += string("'") + db.escape(*i) + "'" ;
	}
	return ret ;
}

bool TSourceDatabase::getPages ( TSourceDatabaseParams &params ) {
	wiki = params.wiki ;
	pages.clear() ;
	TWikidataDB db ( *platform , wiki ) ;
	
	vector <vector<string> > cat_pos ;
	for ( auto i = params.positive.begin() ; i != params.positive.end() ; i++ ) {
		vector <string> x ;
		getCategoriesInTree ( db , i->name , i->depth , x ) ;
		if ( x.size() == 0 ) continue ;
		cat_pos.push_back ( x ) ;
	}
	
	if ( cat_pos.size() == 0 ) {
		cout << "No cats\n" ;
		exit(0) ;
	}
	
	string sql ;
	if ( params.combine == "subset" ) {
//		sql = "SELECT DISTINCT p.*" ;
		sql = "select distinct p.page_id,p.page_title,p.page_namespace" ;

//		sql += ",group_concat(DISTINCT cl0.cl_to SEPARATOR '|') AS cats" ;
		sql += " FROM ( SELECT * from categorylinks where cl_to IN (" ;
		sql += listEscapedStrings ( db , cat_pos[0] ) ;
		sql += ")) cl0" ;
		for ( uint32_t a = 1 ; a < cat_pos.size() ; a++ ) {
			char tmp[200] ;
			sprintf ( tmp , " INNER JOIN categorylinks cl%d on cl0.cl_from=cl%d.cl_from and cl%d.cl_to IN (" , a , a , a ) ;
			sql += tmp ;
			sql += listEscapedStrings ( db , cat_pos[a] ) ;
			sql += ")" ;
		}

		sql += " INNER JOIN (page p" ;
	//	if ( $giu ) $sql .= ",globalimagelinks g" ;
		sql += ") on p.page_id=cl0.cl_from" ;
		
		if ( params.page_namespace_ids.size() > 0 ) {
			sql += " AND p.page_namespace IN(" ;
			for ( auto i = params.page_namespace_ids.begin() ; i != params.page_namespace_ids.end() ; i++ ) {
				if ( i != params.page_namespace_ids.begin() ) sql += "," ;
				char tmp[200] ;
				sprintf ( tmp , "%d" , *i ) ;
				sql += tmp ;
			}
			sql += ")" ;
		}
		
	//	if ( $giu ) $sql .= " AND gil_to=page_title" ;
		if ( params.redirects == "only" ) sql += " AND page_is_redirect=1" ;
		if ( params.redirects == "none" ) sql += " AND page_is_redirect=0" ;
	}

	sql += " GROUP BY p.page_id" ; // Could return multiple results per page in normal search, thus making this GROUP BY general
//	if ( $giu ) $sql .= ",gil_wiki,gil_page" ;
//	sql += " ORDER BY cl0.cl_sortkey" ;

	cout << sql << endl ;
	
	TPageList pl1 ;
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
//	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	uint32_t cnt = 0 ;
	while ((row = mysql_fetch_row(result))) {
		pl1.pages.push_back ( TPage ( row[1] , atoi(row[2]) ) ) ;
//		string id = row[0] ;
//		if ( tmp.find(cat) != tmp.end() ) continue ; // Already had that
//		new_cats.push_back ( cat ) ;
//		tmp[cat] = true ; // Had that
	}
	mysql_free_result(result);

	cout << "Got " << pl1.pages.size() << " pages\n" ;	
	
	
	pl1.pages.swap ( pages ) ;
}


void TSourceDatabase::goDepth ( TWikidataDB &db , map <string,bool> &tmp , vector <string> &cats , int16_t left ) {
	if ( left <= 0 ) return ; // Maximum depth reached
	if ( cats.size() == 0 ) return ; // Nothing to do
	string sql = "SELECT DISTINCT page_title FROM page,categorylinks WHERE cl_from=page_id AND cl_type='subcat' AND cl_to IN (" ;
	for ( auto i = cats.begin() ; i != cats.end() ; i++ ) {
		if ( i != cats.begin() ) sql += "," ;
		sql += string("'") + db.escape(*i) + "'" ;
		tmp[*i] = true ; // Had that
	}
	sql += ")" ;

//	cout << sql << endl ;
	
	vector <string> new_cats ;
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
//	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	uint32_t cnt = 0 ;
	while ((row = mysql_fetch_row(result))) {
		string cat = row[0] ;
		if ( tmp.find(cat) != tmp.end() ) continue ; // Already had that
		new_cats.push_back ( cat ) ;
		tmp[cat] = true ; // Had that
	}
	mysql_free_result(result);
	
//	cout << "Subcats: " << new_cats.size() << ", left: " << left << endl ;
	
	goDepth ( db , tmp , new_cats , left-1 ) ;
}

void TSourceDatabase::getCategoriesInTree ( TWikidataDB &db , string name , int16_t depth , vector <string> &ret ) {
	
	map <string,bool> tmp ;
	name = db.space2_ ( name ) ;
	name[0] = toupper(name[0]) ;
	
	vector <string> cats_init ;
	cats_init.push_back ( name ) ;
	tmp[name] = true ;
	db.doConnect() ;
	goDepth ( db , tmp , cats_init , depth ) ;
	
	ret.clear() ;
	ret.reserve ( tmp.size() ) ;
	for ( auto i = tmp.begin() ; i != tmp.end() ; i++ ) {
		cout << "FOUND: " << i->first << endl ;
		ret.push_back ( i->first ) ;
	}
}






//________________________________________________________________________________________________________________________

bool TPlatform::readConfigFile ( string filename ) {
	char * buffer;
	size_t result;
  	FILE *pFile = fopen ( filename.c_str() , "rb" );
	if (pFile==NULL) {error ("Cannot open config file"); return false ; }
	
	fseek (pFile , 0 , SEEK_END);
	long lSize = ftell (pFile);
	rewind (pFile);
	
	buffer = (char*) malloc (sizeof(char)*lSize);
	if (buffer == NULL) {error ("Memory error"); return false ;}

	result = fread (buffer,1,lSize,pFile);
	if (result != lSize) {error ("Reading error"); return false ;}
	fclose (pFile);

	MyJSON j ( buffer ) ;
	free (buffer);
	
	for ( auto i = j.o.begin() ; i != j.o.end() ; i++ ) {
		if ( i->first == "" ) continue ;
		config[i->first] = i->second.s ;
	}

	return true ;
}
