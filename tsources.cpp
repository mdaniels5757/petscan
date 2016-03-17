#include "main.h"

string TPageList::getNamespaceString ( const int16_t ns ) {
	loadNamespaces() ;
	if ( ns_local.find(ns) != ns_local.end() ) return ns_local[ns] ;
	return "" ; // Unknown
}

int16_t TPageList::getNamespaceNumber ( const string &ns ) {
	loadNamespaces() ;
	return 0 ;
}

void TPageList::loadNamespaces () {
	if ( namespaces_loaded ) return ;
	if ( wiki.empty() ) return ;
	namespaces_loaded = true ;
	// Load Namespace JSON
	MyJSON j ;
	string url = "https://" + getWikiServer ( wiki ) + "/w/api.php?action=query&meta=siteinfo&siprop=namespaces|namespacealiases&format=json" ;
	if ( !loadJSONfromURL ( url , j ) ) return ;
	
	for ( auto i = j["query"]["namespaces"].o.begin() ; i != j["query"]["namespaces"].o.end() ; i++ ) {
		uint16_t num = atoi ( i->first.c_str() ) ;
		map <string,MyJSON> *v = &(i->second.o ) ;
		if ( v->find("canonical") != v->end() ) {
			ns_string2id[(*v)["canonical"].s] = num ;
			ns_canonical[num] = (*v)["canonical"].s ;
		}
		ns_string2id[(*v)["*"].s] = num ;
		ns_local[num] = (*v)["*"].s ;
	}
	
	// TODO aliases
}

void TPageList::customSort ( uint8_t mode , bool ascending ) {
//cout << "SORTING BY " << (int)mode << ", " << (ascending?"ASC":"DESC") << endl ;

	switch ( mode ) {
		case PAGE_SORT_DEFAULT : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.id<b.meta.id;} ) ; break ;
		case PAGE_SORT_TITLE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.name<b.name;} ) ; break ; // FIXME namespaces
		case PAGE_SORT_NS_TITLE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.name<b.name;} ) ; break ; // FIXME namespaces
		case PAGE_SORT_SIZE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.size<b.meta.size;} ) ; break ;
		case PAGE_SORT_DATE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.timestamp<b.meta.timestamp;} ) ; break ;
	}
	
	if ( !ascending ) reverse ( pages.begin() , pages.end() ) ;
}

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

	if ( cat_pos.empty() || cat_pos[0].empty() ) {
		cout << "No categories\n" ;
		return false ;
	}
	
	string sql ;
	if ( params.combine == "subset" ) {
//		sql = "SELECT DISTINCT p.*" ;
		sql = "select distinct p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;

//		sql += ",group_concat(DISTINCT cl0.cl_to SEPARATOR '|') AS cats" ;
		sql += " FROM ( SELECT * from categorylinks where cl_to IN (" ;
		sql += db.space2_ ( listEscapedStrings ( db , cat_pos[0] ) ) ;
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

//	cout << sql << endl ;
	
	TPageList pl1 ( wiki ) ;
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
//	int num_fields = mysql_num_fields(result);
	MYSQL_ROW row;
	uint32_t cnt = 0 ;
	while ((row = mysql_fetch_row(result))) {
		int16_t nsnum = atoi(row[2]) ;
		string nsname = pl1.getNamespaceString ( nsnum ) ;
		string title ( row[1] ) ;
		if ( !nsname.empty() ) title = nsname + ":" + title ;

		TPage p ( title , nsnum ) ;
		p.meta.id = atol(row[0]) ;
		p.meta.size = atol(row[4]) ;
		p.meta.timestamp = row[3] ;
//		strcpy ( p.meta.timestamp , row[3] ) ;
		
		pl1.pages.push_back ( p ) ;
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
//		cout << "FOUND: " << i->first << endl ;
		ret.push_back ( i->first ) ;
	}
}


//________________________________________________________________________________________________________________________

