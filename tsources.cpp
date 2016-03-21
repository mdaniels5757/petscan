#include "main.h"
#include <set>

const string TPage::getNameWithoutNamespace() {
	if ( meta.ns == 0 ) return name ;
	return name.substr ( name.find_first_of(':')+1 ) ;
}


string TPageMetadata::getMisc ( const string &key , const string &_default ) {
	return misc.find(key)==misc.end() ? _default : misc[key] ;
}


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

//________________________________________________________________________________________________________________________


bool TSource::error ( string s ) {
	if ( platform ) platform->error ( s ) ;
	return false ;
}


//________________________________________________________________________________________________________________________


bool TSourceSPARQL::runQuery ( string query ) {
	clear() ;
	wiki = "wikidatawiki" ;
	
	MyJSON j ;
	string url = "https://query.wikidata.org/sparql?format=json&query=" + escapeURLcomponent ( sparql_prefixes + query ) ;
	if ( !loadJSONfromURL ( url , j ) ) return error ( "JSON read error for " + url ) ;

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
	MyJSON j ;
	char s[200] ;
	sprintf ( s , "https://tools.wmflabs.org/pagepile/api.php?id=%d&action=get_data&format=json&doit" , id ) ;
	string url = s ;
	if ( !loadJSONfromURL ( url , j ) ) return error ( "JSON read error for " + url ) ;

	clear() ;
	wiki = j["wiki"].s ;
	for ( uint32_t i = 0 ; i < j["pages"].size() ; i++ ) pages.push_back ( TPage ( j["pages"][i].s , 0 ) ) ;
	return true ;
}

//________________________________________________________________________________________________________________________


string TSourceDatabase::listEscapedStrings ( TWikidataDB &db , vector <string> &s , bool fix_spaces ) {
	string ret ;
//	cout << "Merging " << s.size() << " entries...\n" ;
	for ( auto i = s.begin() ; i != s.end() ; i++ ) {
		if ( i != s.begin() ) ret += "," ;
		ret += string("'") + db.escape(fix_spaces?space2_(*i):*i) + "'" ;
	}
	return ret ;
}

bool TSourceDatabase::parseCategoryList ( TWikidataDB &db , vector <TSourceDatabaseCatDepth> &input , vector <vector<string> > &output ) {
	output.clear() ;
	for ( auto i = input.begin() ; i !=input.end() ; i++ ) {
		vector <string> x ;
		getCategoriesInTree ( db , i->name , i->depth , x ) ;
		if ( x.size() == 0 ) continue ;
		output.push_back ( x ) ;
	}
	return !( output.empty() || output[0].empty() ) ;
}

string TSourceDatabase::templateSubquery ( TWikidataDB &db , vector <string> input , bool use_talk_page , bool find_not ) {
	string ret ;
	
	// NOTE: uses different mechanisms
	if ( use_talk_page ) {
		if ( find_not ) ret += " AND NOT EXISTS " ;
		else ret += " AND EXISTS " ;
		ret += "(SELECT * FROM templatelinks,page pt WHERE MOD(p.page_namespace,2)=0 AND pt.page_title=p.page_title AND pt.page_namespace=p.page_namespace+1 AND tl_from=pt.page_id AND tl_namespace=10 AND tl_title" ;
	} else {
		if ( find_not ) ret += " AND p.page_id NOT IN " ;
		else ret += " AND p.page_id IN " ;
		ret += "(SELECT DISTINCT tl_from FROM templatelinks WHERE tl_namespace=10 AND tl_title" ;
//		ret += "(SELECT * FROM templatelinks WHERE tl_from=page_id AND tl_namespace=10 AND tl_title" ;
	}
	
	if ( input.size() > 1 ) {
		ret += " IN (" + listEscapedStrings ( db , input ) + ")" ;
	} else {
		ret += "=" + listEscapedStrings ( db , input ) ;
	}

	ret += ")" ;
	return ret ;
}

string TSourceDatabase::linksFromSubquery ( TWikidataDB &db , vector <string> input ) { // TODO speed up (e.g. IN ()); pages from all namespaces?
	string ret ;
	ret += "( SELECT p_to.page_id FROM page p_to,page p_from,pagelinks WHERE p_from.page_namespace=0 AND p_from.page_id=pl_from AND pl_namespace=p_to.page_namespace AND pl_title=p_to.page_title AND p_from.page_title" ;
//	ret += "(SELECT * FROM pagelinks,page pfrom WHERE pfrom.page_id=pl_from AND pl_title=p.page_title AND pl_from_namespace=p.page_namespace AND pl_namespace=0 AND pfrom.page_title" ;
	
	if ( input.size() > 1 ) {
		ret += " IN (" + listEscapedStrings ( db , input ) + ")" ;
	} else {
		ret += "=" + listEscapedStrings ( db , input ) ;
	}

	ret += ")" ;
	return ret ;
}

bool TSourceDatabase::getPages ( TSourceDatabaseParams &params ) {
	wiki = params.wiki ;
	pages.clear() ;
	TWikidataDB db ( *platform , wiki ) ;
	
	vector <vector<string> > cat_pos , cat_neg ;
	bool has_pos_cats = parseCategoryList ( db , params.positive , cat_pos ) ;
	bool has_neg_cats = parseCategoryList ( db , params.negative , cat_neg ) ;
	bool has_pos_templates = params.templates_yes.size()+params.templates_any.size() > 0 ;
	bool has_pos_linked_from = params.linked_from_all.size()+params.linked_from_any.size() > 0 ;

	string primary ;
	if ( has_pos_cats ) primary = "categories" ;
	else if ( has_pos_templates ) primary = "templates" ;
	else if ( has_pos_linked_from ) primary = "links_from" ;
	else {
		cout << "No starting point\n" ;
		return false ;
	}
	
	string sql ;
	
	if ( primary == "categories" ) {
		if ( params.combine == "subset" ) {
			sql = "select distinct p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;

	//		sql += ",group_concat(DISTINCT cl0.cl_to SEPARATOR '|') AS cats" ;
			sql += " FROM ( SELECT * from categorylinks where cl_to IN (" ;
			sql += space2_ ( listEscapedStrings ( db , cat_pos[0] ) ) ;
			sql += ")) cl0" ;
			for ( uint32_t a = 1 ; a < cat_pos.size() ; a++ ) {
				char tmp[200] ;
				sprintf ( tmp , " INNER JOIN categorylinks cl%d on cl0.cl_from=cl%d.cl_from and cl%d.cl_to IN (" , a , a , a ) ;
				sql += tmp ;
				sql += listEscapedStrings ( db , cat_pos[a] ) ;
				sql += ")" ;
			}

		} else if ( params.combine == "union" ) {
			cout << "UNION!\n" ;
			
			// Merge and unique subcat list
			vector <string> tmp ;
			for ( uint32_t a = 0 ; a < cat_pos.size() ; a++ ) {
				tmp.insert ( tmp.end() , cat_pos[a].begin() , cat_pos[a].end() ) ;
			}
			set <string> s ( tmp.begin() , tmp.end() ) ;
			tmp.assign ( s.begin() , s.end() ) ;
			
			sql = "select distinct p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
			sql += " FROM ( SELECT * from categorylinks where cl_to IN (" ;
			sql += listEscapedStrings ( db , tmp ) ;
			sql += ")) cl0" ;
			cout << sql << endl ;
		}

		sql += " INNER JOIN (page p" ;
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
	


	} else if ( primary == "templates" || primary == "links_from" ) {
		sql = "select distinct p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
		sql += " FROM page WHERE 1=1" ;
	}
	
	// Negative categories
	if ( has_neg_cats ) {
		for ( auto i = cat_neg.begin() ; i != cat_neg.end() ; i++ ) {
			sql += " AND NOT EXISTS (SELECT * FROM categorylinks WHERE cl_from=p.page_id AND cl_to IN (" ;
			sql += listEscapedStrings ( db , *i ) ;
			sql += "))" ;
		}
	}
	
	// Templates as secondary; template namespace only!
	// TODO talk page
	if ( has_pos_templates ) {
		// All
		for ( auto i = params.templates_yes.begin() ; i != params.templates_yes.end() ; i++ ) {
			sql += templateSubquery ( db , { db.escape(*i) } , params.templates_yes_talk_page , false ) ;
		}
		
		// Any
		if ( !params.templates_any.empty() ) {
			sql += templateSubquery ( db , params.templates_any , params.templates_any_talk_page , false ) ;
		}
	}
	
	// Negative templates
	if ( !params.templates_no.empty() ) {
		sql += templateSubquery ( db , params.templates_no , params.templates_no_talk_page , true ) ;
	}
	
	// Links from
	for ( auto i = params.linked_from_all.begin() ; i != params.linked_from_all.end() ; i++ ) {
		sql += " AND page_id IN " + linksFromSubquery ( db , { db.escape(*i) } ) ; // " AND EXISTS "
	}
	if ( !params.linked_from_any.empty() ) sql += " AND page_id IN " + linksFromSubquery ( db , params.linked_from_any ) ; // " AND EXISTS "
	if ( !params.linked_from_none.empty() ) sql += " AND page_id NOT IN " + linksFromSubquery ( db , params.linked_from_none ) ; // " AND NOT EXISTS "


	// Last edit
	if ( params.last_edit_anon == "yes" ) sql += " AND EXISTS (SELECT * FROM revision WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=0)" ;
	if ( params.last_edit_anon == "no" ) sql += " AND EXISTS (SELECT * FROM revision WHERE rev_id=page_latest AND rev_page=page_id AND rev_user!=0)" ;
	if ( params.last_edit_bot == "yes" ) sql += " AND EXISTS (SELECT * FROM revision,user_groups WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=ug_user AND ug_group='bot')" ;
	if ( params.last_edit_bot == "no" ) sql += " AND NOT EXISTS (SELECT * FROM revision,user_groups WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=ug_user AND ug_group='bot')" ;
//	if ( params.last_edit_flagged == "yes" ) sql += " AND EXISTS (SELECT * FROM flaggedrevs WHERE fr_rev_id=page_latest AND fr_page_id=page_id)" ;
//	if ( params.last_edit_flagged == "no" ) sql += " AND NOT EXISTS (SELECT * FROM flaggedrevs WHERE fr_rev_id=page_latest AND fr_page_id=page_id)" ;
	if ( params.last_edit_flagged == "yes" ) sql += " AND EXISTS (SELECT * FROM flaggedpages WHERE page_id=fp_page_id AND fp_stable=page_latest AND fp_reviewed=1)" ;
	if ( params.last_edit_flagged == "no" ) sql += " AND EXISTS (SELECT * FROM flaggedpages WHERE page_id=fp_page_id AND fp_stable=page_latest AND fp_reviewed!=1)" ;


	// Misc
	if ( params.redirects == "only" ) sql += " AND p.page_is_redirect=1" ;
	if ( params.redirects == "no" ) sql += " AND p.page_is_redirect=0" ;

	sql += " GROUP BY p.page_id" ; // Could return multiple results per page in normal search, thus making this GROUP BY general
	cout << sql << endl ;
	
	struct timeval before , after;
	gettimeofday(&before , NULL);

	TPageList pl1 ( wiki ) ;
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	
	gettimeofday(&after , NULL);
	printf ( "Query time %2.2fs\n" , time_diff(before , after)/1000000 ) ;
	
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
	return true ;
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
	name = space2_ ( name ) ;
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

