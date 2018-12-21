#include "main.h"
#include <set>
#include <time.h>


//________________________________________________________________________________________________________________________


bool TSource::error ( string s ) {
	if ( platform ) platform->error ( s ) ;
	return false ;
}


//________________________________________________________________________________________________________________________



bool TSourceLabels::run () {
	clear() ;
	wiki = "wikidatawiki" ;
	TWikidataDB db ( wiki , platform ) ;
	if ( !db.isConnected() ) return false ;
	string sql = platform->getLabelBaseSQL ( db ) ;
	if ( sql.empty() ) return false ;

	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		pages.push_back ( TPage ( row[0] , 0 ) ) ;
	}
	run_result = true ;
	return run_result ;
}

//________________________________________________________________________________________________________________________


bool TSourceSPARQL::runQuery ( string query ) {
	if ( query.empty() ) return false ;
	clear() ;
	wiki = "wikidatawiki" ;
	
	string sparql_tool_note = "#TOOL: PetScan\n" ;
	
	json j ;
	string url = "https://query.wikidata.org/sparql?format=json&query=" + escapeURLcomponent ( sparql_tool_note + sparql_prefixes + query ) ;
	if ( !loadJSONfromURL ( url , j ) ) {
		url = "https://query.wikidata.org/#" + escapeURLcomponent ( sparql_prefixes + query ) ;
		return error ( "Error while running SPARQL query. <a class='alert-link' href='"+url+"' target='_blank'>Check if the query works</a>." ) ;
	}
	
	
	string item_key = j["head"]["vars"][0] ;
	pages.reserve ( j["results"]["bindings"].size() ) ;
	for ( uint32_t i = 0 ; i < j["results"]["bindings"].size() ; i++ ) {
		string v = j["results"]["bindings"][i][item_key]["value"] ;
		const char *last , *c ;
		for ( last = NULL , c = v.c_str() ; *c ; c++ ) {
			if ( *c == '/' ) last = c+1 ;
		}
		if ( !last ) continue ;
		pages.push_back ( TPage ( last , 0 ) ) ;
	}

	data_loaded = true ;
	return data_loaded ;
}

bool TSourceSPARQL::run () {
	string query = platform->getParam("sparql","" ) ;
	if ( query.empty() ) return false ;
	run_result = runQuery ( query ) ;
	return run_result ;
}

//________________________________________________________________________________________________________________________


bool TSourcePagePile::getPile ( uint32_t id ) {
	char s[200] ;
	sprintf ( s , "https://tools.wmflabs.org/pagepile/api.php?id=%d&action=get_data&format=json&doit" , id ) ;
	string url = s ;
	json j ;
	if ( !loadJSONfromURL ( url , j , false ) ) return error ( "PagePile retrieval error. PagePile "+ui2s(id)+" might not exists." ) ;
	clear() ;
	wiki = j["wiki"] ;
	for ( auto &p: j["pages"] ) {
		string name = p ;
		TPage np ( name ) ;
		np.determineNamespace ( this ) ;
		pages.push_back ( np ) ;
	}

	data_loaded = true ;
	return true ;
}

bool TSourcePagePile::run () {
	string pile = platform->getParam("pagepile","") ;
	if ( pile.empty() ) return false ;
	run_result = getPile ( atoi ( pile.c_str() ) ) ;
	return run_result ;
}


//________________________________________________________________________________________________________________________

bool TSourceSearch::run () {
	string search_query = platform->getParam("search_query","") ;
	if ( search_query.empty() ) return false ;
	string search_wiki = platform->getParam("search_wiki","") ;
	int search_max_results = atoi ( platform->getParam("search_max_results","500").c_str() ) ;

	string server = getWikiServer ( search_wiki ) ;
	if ( server == "NO_SERVER_FOUND" ) return false ;
	wiki = search_wiki ;

	string search_namespace = search_wiki == "commonswiki" ? "6" : "0"; // search for files on Commons

	clear() ;
	int srstart = 0 ;
	while ( pages.size() < search_max_results ) {
		string url = "https://" + server + "/w/api.php?action=query&list=search&srnamespace=" + search_namespace + "&srlimit=500&format=json&srsearch=" + urlencode(search_query) ;
		if ( srstart > 0 ) {
			char s[200] ;
			sprintf ( s , "&sroffset=%d" , srstart ) ;
			url += s ;
		}
		json j ;
//cout << url << endl ;
		if ( !loadJSONfromURL ( url , j , false ) ) return error ( "Search retrieval error for " + url ) ;
		int pages_found = 0 ;
		for ( auto &p: j["query"]["search"] ) {
			TPage np ( p["title"] , p["ns"] ) ;
			np.determineNamespace ( this ) ;
			pages.push_back ( np ) ;
			pages_found++ ;
			if ( pages.size() >= search_max_results ) break ;
		}
		if ( pages_found < 500 ) break ; // This is the end, my friend, the end
		srstart += pages_found ;
	}

	data_loaded = true ;
	return true ;
}


//________________________________________________________________________________________________________________________

bool TSourceWikidata::getData ( string sites ) {
	clear() ;
	sites = trim ( sites ) ;
	if ( sites.empty() ) return false ;
	
	wiki = "wikidatawiki" ;
	TWikidataDB db ( wiki , platform ) ;
	if ( !db.isConnected() ) return false ;

	bool no_statements = !platform->getParam("wpiu_no_statements","").empty() ;
	
	vector <string> v ;
	split ( sites , v , ',' ) ;
	if ( v.empty() ) return false ;
	string sql = "SELECT ips_item_id FROM wb_items_per_site" ;
	if ( no_statements ) sql += ",page_props,page" ;
	sql += " WHERE ips_site_id IN (" + TSourceDatabase::listEscapedStrings ( db , v ) + ")" ;
	
	if ( no_statements ) {
		sql += " AND page_namespace=0 AND ips_item_id=substr(page_title,2)*1 AND page_id=pp_page AND pp_propname='wb-claims' AND pp_sortkey=0" ;
	}
	
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	pages.reserve ( mysql_num_rows(result) ) ;
	MYSQL_ROW row;
	uint32_t cnt = 0 ;
	while ((row = mysql_fetch_row(result))) {
		pages.push_back ( TPage ( "Q"+string(row[0]) , 0 ) ) ;
	}
	mysql_free_result(result);

	
	return true ;
}

bool TSourceWikidata::run () {
	run_result = getData ( platform->getParam("wikidata_source_sites","") ) ;
	return run_result ;
}

//________________________________________________________________________________________________________________________

bool TSourceManual::parseList ( string &text , string &new_wiki ) {
	wiki = new_wiki ;
	vector <string> v ;
	split ( text , v , '\n' ) ;
	for ( auto row: v ) {
		if ( trim(row).empty() ) continue ;
		TPage np ( row ) ;
		np.determineNamespace ( this ) ;
		pages.push_back ( np ) ;
	}

	data_loaded = true ;
	return data_loaded ;
}

bool TSourceManual::run () {
	string text = platform->getParam("manual_list","") ;
	string wiki = platform->getParam("manual_list_wiki","") ;
	if ( text.empty() || wiki.empty() ) return false ;
	run_result = parseList ( text , wiki ) ;
	return run_result ;
}

//________________________________________________________________________________________________________________________


string TSourceDatabase::listEscapedStrings ( TWikidataDB &db , vector <string> &s , bool fix_spaces ) {
	string ret ;
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

void TSourceDatabase::groupLinkListByNamespace ( vector <string> &input , map <int32_t,vector <string> > &nslist ) {
	for ( auto title:input ) {
		vector <string> v ;
		split ( trim(title) , v , ':' , 2 ) ;
		int32_t ns = 0 ;
		if ( v.size() == 2 ) {
			ns = getNamespaceNumber ( trim(v[0]) ) ;
			title = trim(v[1]) ;
		}
		nslist[ns].push_back ( title ) ;
	}
}

string TSourceDatabase::linksFromSubquery ( TWikidataDB &db , vector <string> input ) { // TODO speed up (e.g. IN ()); pages from all namespaces?
	map <int32_t,vector <string> > nslist ;
	groupLinkListByNamespace ( input , nslist ) ;
	
	string ret ;
	for ( auto nsgroup:nslist ) {
		if ( !ret.empty() ) ret += " ) OR ( " ;
		ret += "( SELECT p_to.page_id FROM page p_to,page p_from,pagelinks WHERE p_from.page_namespace=" + ui2s(nsgroup.first) + " AND p_from.page_id=pl_from AND pl_namespace=p_to.page_namespace AND pl_title=p_to.page_title AND p_from.page_title" ;
	
		if ( nsgroup.second.size() > 1 ) {
			ret += " IN (" + listEscapedStrings ( db , nsgroup.second ) + ")" ;
		} else {
			ret += "=" + listEscapedStrings ( db , nsgroup.second ) ;
		}

		ret += ")" ;
	}
	ret = "(" + ret + ")" ;
	return ret ;
}

string TSourceDatabase::linksToSubquery ( TWikidataDB &db , vector <string> input ) { // TODO speed up (e.g. IN ()); pages from all namespaces?
	map <int32_t,vector <string> > nslist ;
	groupLinkListByNamespace ( input , nslist ) ;
	
	string ret ;
	for ( auto nsgroup:nslist ) {
		if ( !ret.empty() ) ret += " ) OR ( " ;
		ret += "( SELECT DISTINCT pl_from FROM pagelinks WHERE pl_namespace=" + ui2s(nsgroup.first) + " AND pl_title" ;
	
		if ( nsgroup.second.size() > 1 ) {
			ret += " IN (" + listEscapedStrings ( db , nsgroup.second ) + ")" ;
		} else {
			ret += "=" + listEscapedStrings ( db , nsgroup.second ) ;
		}

		ret += ")" ;
	}
	ret = "(" + ret + ")" ;
	return ret ;
}

bool TSourceDatabase::run () {
	platform->setDatabaseParameters ( params ) ;
	run_result = getPages() ;
	return run_result ;
}

bool TSourceDatabase::getPages () {
	wiki = (primary_pagelist) ? primary_pagelist->wiki : params.wiki ;
	pages.clear() ;
	TWikidataDB db ( wiki , platform ) ;
	if ( !db.isConnected() ) return false ;

	vector <vector<string> > cat_pos , cat_neg ;
	bool has_pos_cats = parseCategoryList ( db , params.positive , cat_pos ) ;
	bool has_neg_cats = parseCategoryList ( db , params.negative , cat_neg ) ;
	bool has_pos_templates = params.templates_yes.size()+params.templates_any.size() > 0 ;
	bool has_pos_linked_from = params.linked_from_all.size()+params.linked_from_any.size()+params.links_to_all.size()+params.links_to_any.size() > 0 ;

	string primary ;
	if ( has_pos_cats ) primary = "categories" ;
	else if ( has_pos_templates ) primary = "templates" ;
	else if ( has_pos_linked_from ) primary = "links_from" ;
	else if ( primary_pagelist ) primary = "pagelist" ;
	else if ( params.page_wikidata_item == "without" ) primary = "no_wikidata" ;
	else {
//		cout << "No starting point for DB\n" ;
		return false ;
	}
	
	
	string lc ;
	if ( params.minlinks > -1 || params.maxlinks > -1 ) {
		lc = ",(SELECT count(*) FROM pagelinks WHERE pl_from=p.page_id) AS link_count" ;
	}

	string sql_before_after ;
	bool is_before_after_done = false ;

	if ( !params.max_age.empty() ) {
		time_t now = time(0) ; // Seconds
		int hours = atoi ( params.max_age.c_str() ) ; // Hours
		now -= hours*60*60 ;
		struct tm *utc = gmtime ( &now ) ; // Will apparently be deleted by system later on
		string after = ui2s(utc->tm_year+1900) + pad(ui2s(utc->tm_mon+1),2,'0') + pad(ui2s(utc->tm_mday),2,'0') + pad(ui2s(utc->tm_hour),2,'0') + pad(ui2s(utc->tm_min),2,'0') + pad(ui2s(utc->tm_sec),2,'0') ;
		params.before = "" ;
		params.after = after ;
	}
	
	if ( params.before+params.after == "" ) {
		is_before_after_done = true ;
	} else {
		sql_before_after = " INNER JOIN (revision r) on r.rev_page=p.page_id" ;
		if ( params.only_new_since ) sql_before_after += " AND r.rev_parent_id=0" ;
		if ( !params.before.empty() ) sql_before_after += " AND rev_timestamp<='"+db.escape(params.before)+"'" ;
		if ( !params.after.empty() ) sql_before_after += " AND rev_timestamp>='"+db.escape(params.after)+"'" ;
		sql_before_after += " " ;
	}

	
	string sql ;
	
	if ( primary == "categories" ) {
		if ( params.combine == "subset" ) {
			sql = "SELECT DISTINCT p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
			sql += lc ;
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
			
			// Merge and unique subcat list
			vector <string> tmp ;
			for ( uint32_t a = 0 ; a < cat_pos.size() ; a++ ) {
				tmp.insert ( tmp.end() , cat_pos[a].begin() , cat_pos[a].end() ) ;
			}
			set <string> s ( tmp.begin() , tmp.end() ) ;
			tmp.assign ( s.begin() , s.end() ) ;
			
			sql = "SELECT DISTINCT p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
			sql += lc ;
			sql += " FROM ( SELECT * FROM categorylinks WHERE cl_to IN (" ;
			sql += listEscapedStrings ( db , tmp ) ;
			sql += ")) cl0" ;
			if(DEBUG_OUTPUT) cout << sql << endl ;
		}

		sql += " INNER JOIN (page p" ;
		sql += ") ON p.page_id=cl0.cl_from" ;
	
	} else if ( primary == "no_wikidata" ) {

		sql = "SELECT DISTINCT p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
		sql += lc ;
		sql += " FROM page p" ;
		if ( !is_before_after_done ) {
			is_before_after_done = true ;
			sql += sql_before_after ;
		}
		sql += " WHERE p.page_id NOT IN (SELECT pp_page FROM page_props WHERE pp_propname='wikibase_item')" ;

	} else if ( primary == "templates" || primary == "links_from" ) {
		sql = "SELECT DISTINCT p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
		sql += " FROM page p" ;
		if ( !is_before_after_done ) {
			is_before_after_done = true ;
			sql += sql_before_after ;
		}
		sql += " WHERE 1=1" ;

	} else if ( primary == "pagelist" ) {
		if ( primary_pagelist->pages.size() == 0 ) return true ; // Nothing to do, but that's OK
		map <int32_t,vector <string> > nslist ;
		for ( auto &p: primary_pagelist->pages ) {
			nslist[p.meta.ns].push_back ( p.getNameWithoutNamespace() ) ;
		}

		vector <string> parts ;
		for ( auto li:nslist ) {
			string part = "(p.page_namespace=" + ui2s(li.first) + " AND p.page_title IN (" + listEscapedStrings ( db , li.second , true ) + "))" ;
			parts.push_back ( part ) ;
		}

		sql = "SELECT DISTINCT p.page_id,p.page_title,p.page_namespace,p.page_touched,p.page_len" ;
		sql += " FROM page p" ;
		if ( !is_before_after_done ) {
			is_before_after_done = true ;
			sql += sql_before_after ;
		}
		sql += " WHERE " ;
		for ( uint32_t cnt = 0 ; cnt < parts.size() ; cnt++ ) {
			if ( cnt > 0 ) sql += " OR " ;
			sql += parts[cnt] ;
		}
	}

	// Namespaces
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

	
	// Negative categories
	if ( has_neg_cats ) {
		for ( auto i = cat_neg.begin() ; i != cat_neg.end() ; i++ ) {
			sql += " AND p.page_id NOT IN (SELECT DISTINCT cl_from FROM categorylinks WHERE cl_to IN (" ;
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
		sql += " AND p.page_id IN " + linksFromSubquery ( db , { db.escape(*i) } ) ; // " AND EXISTS "
	}
	if ( !params.linked_from_any.empty() ) sql += " AND p.page_id IN " + linksFromSubquery ( db , params.linked_from_any ) ; // " AND EXISTS "
	if ( !params.linked_from_none.empty() ) sql += " AND p.page_id NOT IN " + linksFromSubquery ( db , params.linked_from_none ) ; // " AND NOT EXISTS "
	
	
	// Links to
	for ( auto i = params.links_to_all.begin() ; i != params.links_to_all.end() ; i++ ) {
		sql += " AND p.page_id IN " + linksToSubquery ( db , { db.escape(*i) } ) ; // " AND EXISTS "
	}
	if ( !params.links_to_any.empty() ) sql += " AND p.page_id IN " + linksToSubquery ( db , params.links_to_any ) ; // " AND EXISTS "
	if ( !params.links_to_none.empty() ) sql += " AND p.page_id NOT IN " + linksToSubquery ( db , params.links_to_none ) ; // " AND NOT EXISTS "

	// Lead image
	if ( params.page_image == "yes" ) sql += " AND EXISTS (SELECT * FROM page_props WHERE p.page_id=pp_page AND pp_propname IN ('page_image','page_image_free'))" ;
	if ( params.page_image == "free" ) sql += " AND EXISTS (SELECT * FROM page_props WHERE p.page_id=pp_page AND pp_propname='page_image_free')" ;
	if ( params.page_image == "nonfree" ) sql += " AND EXISTS (SELECT * FROM page_props WHERE p.page_id=pp_page AND pp_propname='page_image')" ;
	if ( params.page_image == "no" ) sql += " AND NOT EXISTS (SELECT * FROM page_props WHERE p.page_id=pp_page AND pp_propname IN ('page_image','page_image_free'))" ;

	// ORES
	if ( params.ores_type != "any" && (params.ores_prediction!="any"||params.ores_prob_from!=0||params.ores_prob_to!=0) ) {
		sql += " AND EXISTS (SELECT * FROM ores_classification WHERE p.page_latest=oresc_rev AND oresc_model IN (SELECT oresm_id FROM ores_model WHERE oresm_is_current=1 AND oresm_name='"+db.escape(params.ores_type)+"')" ;
		if ( params.ores_prediction == "yes" ) sql += " AND oresc_is_predicted=1" ;
		if ( params.ores_prediction == "no" ) sql += " AND oresc_is_predicted=0" ;
		if ( params.ores_prob_from != 0 ) sql += " AND oresc_probability>=" + f2s(params.ores_prob_from) ;
		if ( params.ores_prob_to != 1 ) sql += " AND oresc_probability<=" + f2s(params.ores_prob_to) ;
		sql += ")" ;
	}

	// Last edit
	if ( params.last_edit_anon == "yes" ) sql += " AND EXISTS (SELECT * FROM revision WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=0)" ;
	if ( params.last_edit_anon == "no" ) sql += " AND EXISTS (SELECT * FROM revision WHERE rev_id=page_latest AND rev_page=page_id AND rev_user!=0)" ;
	if ( params.last_edit_bot == "yes" ) sql += " AND EXISTS (SELECT * FROM revision,user_groups WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=ug_user AND ug_group='bot')" ;
	if ( params.last_edit_bot == "no" ) sql += " AND NOT EXISTS (SELECT * FROM revision,user_groups WHERE rev_id=page_latest AND rev_page=page_id AND rev_user=ug_user AND ug_group='bot')" ;
	if ( params.last_edit_flagged == "yes" ) sql += " AND NOT EXISTS (SELECT * FROM flaggedpage_pending WHERE p.page_id=fpp_page_id)" ;
	if ( params.last_edit_flagged == "no" ) sql += " AND EXISTS (SELECT * FROM flaggedpage_pending WHERE p.page_id=fpp_page_id)" ;
	
		
	// Misc
	if ( params.redirects == "yes" ) sql += " AND p.page_is_redirect=1" ;
	if ( params.redirects == "no" ) sql += " AND p.page_is_redirect=0" ;
	if ( params.larger > -1 ) sql += " AND p.page_len>=" + ui2s(params.larger) ;
	if ( params.smaller > -1 ) sql += " AND p.page_len<=" + ui2s(params.smaller) ;
	
	// Speed up "Only pages without Wikidata items" for NS0 pages
	if ( primary != "no_wikidata" && params.page_wikidata_item == "without" && params.page_namespace_ids.size() == 1 && params.page_namespace_ids[0] == 0 ) {
		sql += " AND NOT EXISTS (SELECT * FROM wikidatawiki_p.wb_items_per_site WHERE ips_site_id='" + wiki + "' AND ips_site_page=REPLACE(p.page_title,'_',' ') AND p.page_namespace=0 LIMIT 1)" ;
	}

	if ( !is_before_after_done ) {
		sql += sql_before_after ;
		is_before_after_done = true ;
	}

	vector <string> having ;	
	if ( params.minlinks > -1 ) having.push_back ( "link_count>=" + ui2s(params.minlinks) ) ;
	if ( params.maxlinks > -1 ) having.push_back ( "link_count<=" + ui2s(params.maxlinks) ) ;

	if ( !having.empty() ) {
		sql += " HAVING" ;
		for ( auto i = having.begin() ; i != having.end() ; i++ ) {
			sql += i == having.begin() ? " " : " AND " ;
			sql += *i ;
		}
	}
	
//	cout << sql << endl ;

	struct timeval before , after;
	gettimeofday(&before , NULL);

	TPageList pl1 ( wiki ) ;
	MYSQL_RES *result = db.getQueryResults ( sql ) ;

//	cout << "Query is done.\n" ;
	
	if ( !result ) {
		cerr << "On wiki " << wiki << ", SQL query failed: " << sql << endl ;
		return error ( "Database query failed. Problem with " + wiki + "?" ) ;
	}
	
	gettimeofday(&after , NULL);
	if(DEBUG_OUTPUT) printf ( "Query time %2.2fs\n" , time_diff(before , after)/1000000 ) ;
	
	MYSQL_ROW row;
	map <uint32_t,bool> hadthat ;
	pl1.pages.reserve ( mysql_num_rows(result) ) ;
	while ((row = mysql_fetch_row(result))) {
		int16_t nsnum = atoi(row[2]) ;
		string nsname = pl1.getNamespaceString ( nsnum ) ;
		string title ( row[1] ) ;
		if ( !nsname.empty() ) title = nsname + ":" + title ;

		TPage p ( title , nsnum ) ;
		p.meta.id = atol(row[0]) ;
		p.meta.size = atol(row[4]) ;
		p.meta.timestamp = row[3] ;
		
		if ( hadthat.find(p.meta.id) != hadthat.end() ) continue ;
		hadthat[p.meta.id] = true ;
		
		pl1.pages.push_back ( p ) ;
	}
	mysql_free_result(result);

//cout << "Getting results is done.\n" ;

	pl1.pages.swap ( pages ) ;

	if(DEBUG_OUTPUT) cout << "Got " << pages.size() << " pages\n" ;	

	data_loaded = true ;
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
//		if(DEBUG_OUTPUT) cout << "FOUND: " << i->first << endl ;
		ret.push_back ( i->first ) ;
	}
}


//________________________________________________________________________________________________________________________

