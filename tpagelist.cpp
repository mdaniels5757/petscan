#include "main.h"

//________________________________________________________________________________________________________________________


void TPage::determineNamespace ( TPageList *pl ) {
	if ( meta.ns != NS_UNKNOWN ) return ; // Already set
	size_t pos = name.find_first_of ( ':' ) ;
	if ( pos == string::npos ) { // No namespace divider, main namespace assumed
		meta.ns = 0 ;
		return ;
	}
	string ns_name = name.substr(0,pos) ;
	if ( ns_name.empty() ) { // ":"-prefixed
		meta.ns = 0 ;
		name = name.substr(1) ;
		return ;
	}
	if ( ns_name[0] >= 'a' && ns_name[0] <= 'z' ) ns_name[0] -= 'a'-'A' ;
	meta.ns = pl->getNamespaceNumber ( ns_name ) ;
}

const string TPage::getNameWithoutNamespace() {
	if ( meta.ns == 0 ) return name ;
	return name.substr ( name.find_first_of(':')+1 ) ;
}


//________________________________________________________________________________________________________________________

string TPageMetadata::getMisc ( const string &key , const string &_default ) {
	return misc.find(key)==misc.end() ? _default : misc[key] ;
}


//________________________________________________________________________________________________________________________


string TPageList::getNamespaceString ( const int16_t ns ) {
	loadNamespaces() ;
	if ( ns_local.find(ns) != ns_local.end() ) return ns_local[ns] ;
	return "" ; // Unknown
}

int16_t TPageList::getNamespaceNumber ( const string &ns ) {
	loadNamespaces() ;
	if ( ns_string2id.find(ns) != ns_string2id.end() ) return ns_string2id[ns] ;
	return 0 ;
}

void TPageList::loadNamespaces () {
	if ( namespaces_loaded ) return ;
	if ( wiki.empty() ) return ;
	namespaces_loaded = true ;

	// Load Namespace JSON
	json j ;
	string url = "https://" + getWikiServer ( wiki ) + "/w/api.php?action=query&meta=siteinfo&siprop=namespaces|namespacealiases&format=json" ;
	if ( !loadJSONfromURL ( url , j ) ) return ;
	
	for ( auto i = j["query"]["namespaces"].begin() ; i != j["query"]["namespaces"].end() ; i++ ) {
		uint16_t num = atoi(i.key().c_str()) ;
		auto v = i.value() ;
		
		if ( v["canonical"].is_string() ) {
			ns_string2id[v["canonical"]] = num ;
			ns_canonical[num] = v["canonical"] ;
		}

		ns_string2id[v["*"]] = num ;
		ns_local[num] = v["*"] ;
	}
	
	// TODO aliases
}

void TPageList::customSort ( uint8_t mode , bool ascending ) {
//cout << "SORTING BY " << (int)mode << ", " << (ascending?"ASC":"DESC") << endl ;

	switch ( mode ) {
		case PAGE_SORT_DEFAULT : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.id<b.meta.id;} ) ; break ;
		case PAGE_SORT_TITLE : std::sort ( pages.begin() , pages.end() , [](TPage a,TPage b){return a.getNameWithoutNamespace()<b.getNameWithoutNamespace();} ) ; break ; // FIXME const
		case PAGE_SORT_NS_TITLE : std::sort ( pages.begin() , pages.end() , [](TPage a,TPage b){return a.meta.ns!=b.meta.ns?(a.meta.ns<b.meta.ns):(a.getNameWithoutNamespace()<b.getNameWithoutNamespace());} ) ; break ; // FIXME const
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

void TPageList::convertToWiki ( string new_wiki ) {
	if ( wiki == new_wiki ) return ; // No conversion required
	if ( wiki != "wikidatawiki" ) convertToWikidata() ;
	convertWikidataToWiki ( new_wiki ) ;
}

void TPageList::convertWikidataToWiki ( string new_wiki ) {
	if ( wiki != "wikidatawiki" ) return ;
	if ( new_wiki == wiki ) return ;

	TWikidataDB db ( string("wikidatawiki") ) ;
	string sql = "SELECT DISTINCT ips_site_page FROM wb_items_per_site WHERE ips_site_id='" + db.escape(new_wiki) + "' AND ips_item_id IN (0" ; // 0 is dummy
	for ( auto &i: pages ) {
		if ( i.meta.ns != 0 ) continue ;
		sql += "," + i.name.substr(1) ;
	}
	sql += ")" ;
	
	TPageList nl ( new_wiki ) ;
	nl.pages.reserve ( size() ) ;
	
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		TPage np ( row[0] ) ;
		np.determineNamespace ( this ) ;
		nl.pages.push_back ( np ) ;
	}
	mysql_free_result(result);
	
	swap ( nl ) ;
}

void TPageList::convertToWikidata () {
	if ( wiki == "wikidatawiki" ) return ; // Well, they all do have an item...

	TWikidataDB db ( string("wikidatawiki") ) ;
	map <string,TPage *> name2o ;
	for ( auto i = pages.begin() ; i != pages.end() ; i++ ) {
		name2o[_2space(i->name)] = &(*i) ;
		if ( name2o.size() < DB_PAGE_BATCH_SIZE ) continue ;
		annotateWikidataItem ( db , wiki , name2o ) ;
	}
	annotateWikidataItem ( db , wiki , name2o ) ;
	
	TPageList nl ( "wikidatawiki" ) ;
	nl.pages.reserve ( size() ) ;
	for ( auto i = pages.begin() ; i != pages.end() ; i++ ) {
		if ( i->meta.q == UNKNOWN_WIKIDATA_ITEM ) continue ;
		nl.pages.push_back ( TPage ( "Q"+ui2s(i->meta.q) , 0 ) ) ;
	}
	swap ( nl ) ;
}

uint32_t TPageList::annotateWikidataItem ( TWikidataDB &db , string wiki , map <string,TPage *> &name2o ) {
	uint32_t ret = 0 ;
	if ( name2o.empty() ) return ret ;
//	cout << "Annotating " << name2o.size() << " pages with items\n" ;
	
	vector <string> tmp ;
	tmp.reserve ( name2o.size() ) ;
	for ( auto i = name2o.begin() ; i != name2o.end() ; i++ ) tmp.push_back ( i->first ) ;
	
	string sql = "SELECT ips_site_page,ips_item_id FROM wb_items_per_site WHERE ips_site_id='" + db.escape(wiki) + "' AND ips_site_page IN (" ;
	sql += TSourceDatabase::listEscapedStrings ( db , tmp , false ) ;
	sql += ")" ;

	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		if ( name2o.find(row[0]) == name2o.end() ) {
			cerr << "TPlatform::annotateWikidataItem: Page not found: " << row[0] << endl ;
			continue ;
		}
		name2o[row[0]]->meta.q = atol(row[1]) ;
		ret++ ;
	}
	mysql_free_result(result);
	
	name2o.clear() ;
	return ret ;
}


void TPageList::join ( string cmd , TPageList &pl ) {
	cout << "me: " << wiki << ", other: " << pl.wiki << endl ;
	if ( !data_loaded ) {
		cout << "Swapping for " << pl.size() << " pages (having " << size() << ")\n" ;
		swap ( pl ) ;
		return ;
	}
	
	cout << cmd << " with " << pl.size() << " pages (having " << size() << "), " ;
	if ( cmd == "intersect" ) intersect ( pl ) ;
	else if ( cmd == "merge" ) merge ( pl ) ;
	cout << " resulting in " << size() << " pages.\n" ;
//	else 
}

void TPageList::swap ( TPageList &pl ) {
	wiki.swap ( pl.wiki ) ;
	pages.swap ( pl.pages ) ;
	{ bool tmp = data_loaded ; data_loaded = pl.data_loaded ; pl.data_loaded = tmp ; }
}

void TPageList::loadMissingMetadata () {
	map <int16_t,vector <TPage *> > ns_page ;
	for ( auto p: pages ) {
		if ( p.meta.id != 0 ) continue ;
		ns_page[p.meta.ns].push_back ( &p ) ;
	}
	if ( ns_page.empty() ) return ;
	
	TWikidataDB db ( wiki ) ;
	
}
