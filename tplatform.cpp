#include "main.h"

void TPlatform::parseCats ( string input , vector <TSourceDatabaseCatDepth> &output ) {
	vector <string> pos ;
	split ( input , pos , '\n' ) ;
	for ( auto i = pos.begin() ; i != pos.end() ; i++ ) {
		if ( i->empty() ) continue ;
		vector <string> parts ;
		split ( (*i) , parts , '|' , 2 ) ;
		if ( parts.empty() ) continue ; // Huh?
		if ( parts.size() < 2 ) parts.push_back ( getParam("depth","0") ) ;
		output.push_back ( TSourceDatabaseCatDepth ( trim(parts[0]) , atoi(parts[1].c_str()) ) ) ;
	}
}

void TPlatform::splitParamIntoVector ( string input , vector <string> &output ) {
	output.clear() ;
	vector <string> v ;
	split ( input , v , '\n' ) ;
	for ( auto i = v.begin() ; i != v.end() ; i++ ) {
		string s = trim ( *i ) ;
		if ( !s.empty() ) {
			s[0] = toupper(s[0]) ;
			output.push_back ( s ) ;
		}
	}
}

string TPlatform::process () {
	struct timeval before , after;
	gettimeofday(&before , NULL);

	TSourceDatabase db ( this ) ;
	TSourceDatabaseParams db_params ;
	db_params.wiki = getWiki() ;
	
	// Get valid namespaces
	for ( auto i = params.begin() ; i != params.end() ; i++ ) {
		string key = i->first ;
		if ( key.length() >= 5 && key[0]=='n' && key[1]=='s' && key[2]=='[' && key[key.length()-1]==']' ) {
			string nss = key.substr(3,key.length()-4) ;
			db_params.page_namespace_ids.push_back ( atoi ( nss.c_str() ) ) ;
		}
	}

	// Legacy parameters
	if ( params.find("comb_subset") != params.end() ) params["combination"] = "subset" ;
	if ( params.find("comb_union") != params.end() ) params["combination"] = "union" ;
	if ( params.find("get_q") != params.end() ) params["wikidata_item"] = "any" ;
	if ( params.find("wikidata") != params.end() ) params["wikidata_item"] = "any" ;
	if ( params.find("wikidata_no_item") != params.end() ) params["wikidata_item"] = "without" ;

	// Add categories
	parseCats ( getParam ( "categories" , "" ) , db_params.positive ) ;
	parseCats ( getParam ( "negcats" , "" ) , db_params.negative ) ;
	
	// Add templates
	splitParamIntoVector ( getParam("templates_yes","") , db_params.templates_yes ) ;
	splitParamIntoVector ( getParam("templates_any","") , db_params.templates_any ) ;
	splitParamIntoVector ( getParam("templates_no" ,"") , db_params.templates_no  ) ;
	if ( getParam("templates_use_talk_yes","") != "" ) db_params.templates_yes_talk_page = true ;
	if ( getParam("templates_use_talk_any","") != "" ) db_params.templates_any_talk_page = true ;
	if ( getParam("templates_use_talk_no" ,"") != "" ) db_params.templates_no_talk_page  = true ;
	
	// Add misc
	db_params.redirects = getParam("show_redirects","both") ;
	db_params.combine = getParam("combination","subset") ;

	// Add linked from
	splitParamIntoVector ( getParam("outlinks_yes","") , db_params.linked_from_all ) ;
	splitParamIntoVector ( getParam("outlinks_any","") , db_params.linked_from_any ) ;
	splitParamIntoVector ( getParam("outlinks_no" ,"") , db_params.linked_from_none  ) ;
	
	
	TPageList pagelist ( getWiki() ) ;
	
	if ( db.getPages ( db_params ) ) {
		pagelist.swap ( db ) ;
	}


	processWikidata ( pagelist ) ;


	// Sort pagelist
//cout << "SORT PARAM: " << getParam("sortby") << "/" << getParam("sortorder") << endl ;
	bool asc = !(getParam("sortorder") == "descending") ;
	if ( getParam("sortby") == "title" ) pagelist.customSort ( PAGE_SORT_TITLE , asc ) ;
	else if ( getParam("sortby") == "ns_title" ) pagelist.customSort ( PAGE_SORT_NS_TITLE , asc ) ;
	else if ( getParam("sortby") == "size" ) pagelist.customSort ( PAGE_SORT_SIZE , asc ) ;
	else if ( getParam("sortby") == "date" ) pagelist.customSort ( PAGE_SORT_DATE , asc ) ;
	else if ( getParam("sortby") == "filesize" ) pagelist.customSort ( PAGE_SORT_FILE_SIZE , asc ) ;
	else if ( getParam("sortby") == "uploaddate" ) pagelist.customSort ( PAGE_SORT_UPLOAD_DATE , asc ) ;
	else if ( getParam("sortby") == "incoming_links" ) pagelist.customSort ( PAGE_SORT_INCOMING_LINKS , asc ) ;
	else pagelist.customSort ( PAGE_SORT_DEFAULT , asc ) ;
	

	gettimeofday(&after , NULL);
	querytime = time_diff(before , after)/1000000 ;

	return renderPageList ( pagelist ) ;
}

void TPlatform::processWikidata ( TPageList &pl ) {
	if ( pl.wiki == "wikidatawiki" ) return ; // Well, they all do have an item...
	string wdi = getParam("wikidata_item","no") ;
	if ( wdi != "any" && wdi != "with" && wdi != "without" ) return ;

	uint32_t with_wikidata_item = 0 ;
	TWikidataDB db ( *this , string("wikidatawiki") ) ;
	map <string,TPage *> name2o ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		name2o[_2space(i->name)] = &(*i) ;
		if ( name2o.size() < DB_PAGE_BATCH_SIZE ) continue ;
		with_wikidata_item += annotateWikidataItem ( db , pl.wiki , name2o ) ;
	}
	with_wikidata_item += annotateWikidataItem ( db , pl.wiki , name2o ) ;
	
	if ( wdi == "any" ) return ; // No further processing
	if ( wdi == "with" and with_wikidata_item == pl.size() ) return ; // Shortcut: All pages have a wikidata item
	
	bool keep_with_item = wdi == "with" ;
	TPageList nl ( pl.wiki ) ;
	nl.pages.reserve ( pl.size() ) ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		if ( keep_with_item && i->meta.q == UNKNOWN_WIKIDATA_ITEM ) continue ;
		if ( !keep_with_item && i->meta.q != UNKNOWN_WIKIDATA_ITEM ) continue ;
		nl.pages.push_back ( *i ) ;
	}
	pl.swap ( nl ) ;
}

uint32_t TPlatform::annotateWikidataItem ( TWikidataDB &db , string wiki , map <string,TPage *> &name2o ) {
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

string TPlatform::getWiki () {
	string l = getParam ( "language" , "en" ) ;
	string p = getParam ( "project" , "wikipedia" ) ;
	if ( l == "wikidata" || p == "wikidata" ) return "wikidatawiki" ;
	if ( l == "commons" || l == "meta" ) return l+"wiki" ;
	if ( p == "wikipedia" ) return l+"wiki" ;
	return l+p ;
}

string TPlatform::getParam ( string key , string default_value ) {
	auto i = params.find(key) ;
	if ( i != params.end() ) return i->second ;
	return default_value ;
}

string TPlatform::renderPageListHTML ( TPageList &pagelist ) {
	content_type = "text/html" ;

	string wdi = getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;
	
	string ret ;
	char tmp[1000] ;
	sprintf ( tmp , "<hr/><h2><a name='results'></a>%ld results</h2>" , pagelist.pages.size() ) ;
	ret += tmp ;
	if ( pagelist.pages.size() == 0 ) return ret ; // No need for empty table
	ret += "<table class='table table-sm table-striped'>" ;
	ret += "<thead><tr><th class='num'>#</th><th class='text-nowrap l_h_title'></th><th class='text-nowrap l_h_id'></th><th class='text-nowrap l_h_namespace'></th><th class='text-nowrap l_h_len'></th><th class='text-nowrap l_h_touched'></th>" ;
	if ( show_wikidata_item ) ret += "<th class='l_h_wikidata'></th>" ;
	ret += "</tr></thead>" ;
	ret += "<tbody>" ;
	uint32_t cnt = 0 ;
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		cnt++ ;
		int16_t ns = i->meta.ns ;
		string nsname = pagelist.getNamespaceString(ns) ;
		if ( nsname.empty() ) nsname = "<span class='l_namespace_0'>Article</span>" ;
		ret += "<tr>" ;
		sprintf ( tmp , "<td class='num'>%d</td>" , cnt ) ;
		ret += tmp ;
		ret += "<td style='width:100%'>" + getLink ( *i ) + "</td>" ;
		sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.id ) ; // ID
		ret += tmp ;
		ret += "<td>"+nsname+"</td>" ; // Namespace name
		sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.size ) ; // Size
		ret += tmp ;
		ret += "<td class='num'>" + string(i->meta.timestamp) + "</td>" ; // TS
		if ( show_wikidata_item ) {
			ret += "<td>" ;
			if ( i->meta.q != UNKNOWN_WIKIDATA_ITEM ) {
				sprintf ( tmp , "Q%d" , i->meta.q ) ;
				ret += "<a target='_blank' href='https://www.wikidata.org/wiki/" ;
				ret += tmp ;
				ret += "'>" ;
				ret += tmp ;
				ret += "</a>" ;
			}
			ret += "</td>" ;
		}
		ret += "</tr>" ;
	}
	ret += "</tbody>" ;
	ret += "</table>" ;
	
	sprintf ( tmp , "<div style='font-size:8pt'>Query took %2.2f seconds.</div>" , querytime ) ;
	ret += tmp ;
	
	return ret ;
}

string TPlatform::renderPageListJSON ( TPageList &pagelist ) {
	string ret ;
	string mode = "catscan" ;
	content_type = "application/json; charset=utf-8" ;
	
	if ( mode == "catscan" ) {
		char tmp[100] ;
		sprintf ( tmp , "%f" , querytime ) ;
		ret += "{" ;
		ret += "\"n\":\"result\"," ;
		ret += "\"a\":{\"querytime_sec\":" + string(tmp) + "}," ;
		ret += "\"*\":[{" ;
		ret += "\"n\":\"combination\"," ;
		ret += "\"a\":{" ;
		ret += "\"type\":\"" + getParam("combination","subset") + "\"," ;
		ret += "\"*\":[" ;
		for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
			if ( i != pagelist.pages.begin() ) ret += "," ;
			ret += "{" ;
			ret += "\"n\":\"page\"," ;
			ret += "\"title\":\"" + MyJSON::escapeString(i->name) + "\"," ;
			ret += "\"id\":\"" + ui2s(i->meta.id) + "\"," ;
			ret += "\"namespace\":\"" + ui2s(i->meta.ns) + "\"," ;
			ret += "\"len\":\"" + ui2s(i->meta.size) + "\"," ;
			ret += "\"touched\":\"" + i->meta.timestamp + "\"," ;
			ret += "\"nstext\":\"" + MyJSON::escapeString(pagelist.getNamespaceString(i->meta.ns)) + "\"" ;
			if ( i->meta.q != UNKNOWN_WIKIDATA_ITEM ) ret += ",\"q\":\"Q" + ui2s(i->meta.q) + "\"" ;
			ret += "}" ;
		}
		ret += "]" ;
		ret += "}" ;
		ret += "}]" ;
		ret += "}" ;
	}
	
	return ret ;
}

string TPlatform::renderPageList ( TPageList &pagelist ) {
	string format = getParam ( "format" , "html" ) ;
	string ret ;
	
	content_type = "text/html; charset=utf-8" ; // Default
	
	if ( format == "json" ) return renderPageListJSON ( pagelist ) ;
	else return renderPageListHTML ( pagelist ) ;
	
	return "" ;
}

string TPlatform::getLink ( const TPage &page ) {
	string label = page.name ;
	string url = page.name ;
	std::replace ( label.begin(), label.end(), '_', ' ') ;
	std::replace ( url.begin(), url.end(), ' ', '_') ;
	// TODO escape '
//	url = urlencode ( url ) ;
	url = "https://" + getWikiServer ( getWiki() ) + "/wiki/" + url ;
	return "<a target='_blank' href='" + url + "'>" + label + "</a>" ;
}

bool TPlatform::readConfigFile ( string filename ) {
	char *buffer = loadFileFromDisk ( filename ) ;
	MyJSON j ( buffer ) ;
	free (buffer);
	
	for ( auto i = j.o.begin() ; i != j.o.end() ; i++ ) {
		if ( i->first == "" ) continue ;
		config[i->first] = i->second.s ;
	}

	return true ;
}
