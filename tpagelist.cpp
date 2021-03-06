#include "main.h"
#include <regex>

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
	if ( !loadJSONfromURL ( url , j , true ) ) return ;
	
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
		case PAGE_SORT_NS_TITLE : std::sort ( pages.begin() , pages.end() , [](TPage a,TPage b){return a.meta.ns!=b.meta.ns?(a.meta.ns<b.meta.ns):(a.getNameWithoutNamespace()<b.getNameWithoutNamespace());} ) ; break ;
		case PAGE_SORT_SIZE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.size<b.meta.size;} ) ; break ;
		case PAGE_SORT_DATE : std::sort ( pages.begin() , pages.end() , [](const TPage &a,const TPage &b){return a.meta.timestamp<b.meta.timestamp;} ) ; break ;
		case PAGE_SORT_REDLINKS_COUNT : std::sort ( pages.begin() , pages.end() , []( TPage a, TPage b){return atoi((a.meta.getMisc("count","0")).c_str())<atoi((b.meta.getMisc("count","0")).c_str());} ) ; break ;
		case PAGE_SORT_INCOMING_LINKS : std::sort ( pages.begin() , pages.end() , []( TPage a,TPage b){return atoi((a.meta.getMisc("incoming_links","0")).c_str())<atoi((b.meta.getMisc("incoming_links","0")).c_str());} ) ; break ;
		case PAGE_SORT_FILE_SIZE : std::sort ( pages.begin() , pages.end() , []( TPage a,TPage b){return atoi((a.meta.getMisc("img_size","0")).c_str())<atoi((b.meta.getMisc("img_size","0")).c_str());} ) ; break ;
		case PAGE_SORT_UPLOAD_DATE : std::sort ( pages.begin() , pages.end() , []( TPage a,TPage b){return (a.meta.getMisc("img_timestamp",""))<(b.meta.getMisc("img_timestamp",""));} ) ; break ;
		case PAGE_SORT_RANDOM : std::random_shuffle ( pages.begin() , pages.end() ) ; break ;
	}
	
	if ( !ascending ) reverse ( pages.begin() , pages.end() ) ;
}

void TPageList::sort () {
	if ( is_sorted ) return ;
	std::sort ( pages.begin() , pages.end() ) ;
}

// Keep only pages in both lists
void TPageList::intersect ( TPageList &pl ) {
	if ( wiki != pl.wiki ) {
		error ( "Intersecting " + wiki + " and " + pl.wiki + " not possible" ) ;
		return ;
	}
	if(DEBUG_OUTPUT) cout << "Intersecting " << size() << " and " << pl.size() << " entries on " << wiki << endl ;
	
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

// Keep pages from either page list
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

// Keep only the pages that are NOT in pl
void TPageList::negate ( TPageList &pl ) {
	if ( wiki != pl.wiki ) {
		error ( "Negating " + wiki + " and " + pl.wiki + " not possible" ) ;
		return ;
	}
	if(DEBUG_OUTPUT) cout << "Negating " << size() << " and " << pl.size() << " entries on " << wiki << endl ;
	
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
		} else if (*first2<*first1) ++first2;
		else {
			++first1; ++first2;
		}
	}
	while ( first1 != last1 ) {
		nl.push_back ( *first1 ) ;
		++first1;
	}
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
	data_loaded = true ;
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
	data_loaded = true ;
}

uint32_t TPageList::annotateWikidataItem ( TWikidataDB &db , string wiki , map <string,TPage *> &name2o ) {
	uint32_t ret = 0 ;
	if ( name2o.empty() ) return ret ;
//	if(DEBUG_OUTPUT) cout << "Annotating " << name2o.size() << " pages with items\n" ;
	
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
	if(DEBUG_OUTPUT) cout << "me: " << wiki << ", other: " << pl.wiki << endl ;
	if ( !data_loaded ) {
		if(DEBUG_OUTPUT) cout << "Swapping for " << pl.size() << " pages (having " << size() << ")\n" ;
		swap ( pl ) ;
		return ;
	}
	
	if(DEBUG_OUTPUT) cout << cmd << " with " << pl.size() << " pages (having " << size() << "), " ;
	if ( cmd == "intersect" ) intersect ( pl ) ;
	else if ( cmd == "merge" ) merge ( pl ) ;
	else if ( cmd == "negate" ) negate ( pl ) ;
	if(DEBUG_OUTPUT) cout << " resulting in " << size() << " pages.\n" ;
}

void TPageList::swap ( TPageList &pl ) {
	wiki.swap ( pl.wiki ) ;
	pages.swap ( pl.pages ) ;
	ns_canonical.swap ( pl.ns_canonical ) ;
	ns_local.swap ( pl.ns_local ) ;
	ns_string2id.swap ( pl.ns_string2id ) ;
	{ bool tmp = data_loaded ; data_loaded = pl.data_loaded ; pl.data_loaded = tmp ; }
}



#define LABEL_BATCH_SIZE 10000

std::mutex g_add_labels_mutex ;

void addLabels ( string &sql , map <string,TPage*> &item2page , TWikidataDB &db ) {
//	TWikidataDB db ( "wikidatawiki" ) ;
	MYSQL_RES *result ;
	{
		std::lock_guard<std::mutex> lock(g_add_labels_mutex);
		result = db.getQueryResults ( sql ) ;
	}
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		if ( string(row[2]) == "alias" ) continue ; // Paranoia
		string name ( row[0] ) ;
		if ( item2page.find(name) == item2page.end() ) continue ; // Say what?
		TPage *p = item2page[name] ;
		p->meta.misc[row[2]] = row[1] ;
	}
	mysql_free_result(result);
}



void TPageList::loadMissingMetadata ( string wikidata_language , TPlatform *platform ) {
	map <int16_t,vector <TPage *> > ns_page ;
	for ( auto &p: pages ) {
		ns_page[p.meta.ns].push_back ( &p ) ;
	}
	if ( ns_page.empty() ) return ;

	TWikidataDB db ( wiki ) ;
	for ( auto i = ns_page.begin() ; i != ns_page.end() ; i++ ) {
		vector < vector<TPage *> > ml ;
		ml.push_back ( vector<TPage *> () ) ;
		for ( auto &j: i->second ) {
			if ( !j->meta.timestamp.empty() ) continue ;
			if ( ml[ml.size()-1].size() >= DB_PAGE_BATCH_SIZE ) ml.push_back ( vector<TPage *> () ) ;
			ml[ml.size()-1].push_back ( j ) ;
		}
		if ( ml.size() == 1 && ml[0].size() == 0 ) continue ; // No need for metadata for any page in this namespace
		for ( auto &batch: ml ) {
			map <string,TPage *> name2page ;
			string sql = "SELECT page_title,page_id,page_len,page_touched FROM page WHERE page_namespace=" + ui2s(i->first) + " AND page_title IN (" ;
			for ( auto j = batch.begin() ; j != batch.end() ; j++ ) {
				string name = (*j)->getNameWithoutNamespace() ;
				name2page[name] = *j ;
				if ( j != batch.begin() ) sql += "," ;
				sql += "'" + db.escape(name) + "'" ;
			}
			sql += ")" ;
			
			MYSQL_RES *result = db.getQueryResults ( sql ) ;
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				string name ( row[0] ) ;
				if ( name2page.find(name) == name2page.end() ) continue ; // Say what?
				TPage *p = name2page[name] ;
				p->meta.id = atol(row[1]) ;
				p->meta.size = atol(row[2]) ;
				p->meta.timestamp = row[3] ;
			}
			mysql_free_result(result);
		}
	}
	if ( wiki != "wikidatawiki" ) return ;
	if ( wikidata_language.empty() ) return ;

	if ( platform && platform->getParam("regexp_filter","").empty() && !platform->getParam("wdf_main","").empty() ) return ; // No need to load labels for WDFIST mode

	addWikidataLabelsForNamespace ( 0 , "item" , wikidata_language , db , ns_page ) ;
	addWikidataLabelsForNamespace ( 120 , "property" , wikidata_language , db , ns_page ) ;
}

void TPageList::addWikidataLabelsForNamespace ( uint32_t namespace_id , string entity_type , string wikidata_language , TWikidataDB &db , map <int16_t,vector <TPage *> > &ns_page ) {
	if ( ns_page.find(namespace_id) == ns_page.end() ) return ; // No NS0 pages=no items
	if ( ns_page[namespace_id].size() == 0 ) return ;

	// Getting labels and descriptions for items (ns0)
	map <string,TPage*> item2page ;
	vector <string> sqls ;
	// term_type IN ('label','description')
	string base_sql = "SELECT term_full_entity_id,term_text,term_type FROM wb_terms WHERE term_entity_type='"+entity_type+"' AND term_language='" + db.escape(wikidata_language) + "' AND term_full_entity_id IN (''" ;
	sqls.push_back ( base_sql ) ;
	uint32_t cnt = 0 ;
	for ( auto i = ns_page[namespace_id].begin() ; i != ns_page[namespace_id].end() ; i++ ) {
		item2page[(*i)->name] = *i ;
		sqls[sqls.size()-1] += ",'" + (*i)->name + "'" ;
		if ( cnt++ < LABEL_BATCH_SIZE ) continue ;
		sqls[sqls.size()-1] += ")" ;
		sqls.push_back ( base_sql ) ;
		cnt = 0 ;
	}
	sqls[sqls.size()-1] += ")" ;

	vector <std::thread *> threads ;
	for ( auto &sql:sqls ) {
		threads.push_back ( new std::thread ( &addLabels , std::ref(sql) , std::ref(item2page) , std::ref(db) ) ) ;
	}
	for ( auto t:threads ) t->join() ;
}

void TPageList::regexpFilter ( string regexp ) {
	if ( trim(regexp).empty() ) return ;
	
	bool is_wikidata = wiki == "wikidatawiki" ;
	std::regex re ( regexp ) ; // icase doesn't work for some reason
	vector <TPage> pl ;
	pl.reserve ( pages.size() ) ;
	for ( auto i = pages.begin() ; i != pages.end() ; i++ ) {
		bool matches ;
		if ( i->meta.ns == 0 && is_wikidata ) {
			string label = i->meta.getMisc ( "label" , "" ) ;
			matches = std::regex_match ( label , re ) ;
		} else {
			string name = _2space ( i->name ) ;
			matches = std::regex_match ( name , re ) ;
		}
		if ( matches ) pl.push_back ( *i ) ;
	}
	pages.swap ( pl ) ;
}
