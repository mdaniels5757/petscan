#include "main.h"
#include <fstream>
#include <regex>
#include <thread>

using json = nlohmann::json;

vector <string> file_data_keys = { "img_size","img_width","img_height","img_media_type","img_major_mime","img_minor_mime","img_user_text","img_timestamp","img_sha1" } ;


void TPlatform::parseCats ( string input , vector <TSourceDatabaseCatDepth> &output ) {
	vector <string> pos ;
	split ( input , pos , '\n' ) ;
	for ( auto i = pos.begin() ; i != pos.end() ; i++ ) {
		if ( i->empty() ) continue ;
		vector <string> parts ;
		split ( (*i) , parts , '|' , 2 ) ;
		if ( parts.empty() ) continue ; // Huh?
		if ( parts.size() < 2 ) parts.push_back ( getParam("depth","0") ) ;
		uint64_t depth = atoi(parts[1].c_str()) ;
		if ( depth > 32000 ) depth = 32000 ; // Someone actually filed a bug report that depth 35000 doesn't work. Seriously, WTF?
		output.push_back ( TSourceDatabaseCatDepth ( trim(parts[0]) , depth ) ) ;
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

void TPlatform::setDatabaseParameters ( TSourceDatabaseParams &db_params ) {
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

	// Last edit
	db_params.last_edit_bot = getParam("edits[bots]","both") ;
	db_params.last_edit_anon = getParam("edits[anons]","both") ;
	db_params.last_edit_flagged = getParam("edits[flagged]","both") ;

	// Page image
	db_params.page_image = getParam("page_image","any") ;

	// ORES
	db_params.ores_type = getParam("ores_type","any") ;
	db_params.ores_prediction = getParam("ores_prediction","any") ;
	db_params.ores_prob_from = atof ( (getParam("ores_prob_from","0",true)).c_str() ) ;
	db_params.ores_prob_to = atof ( (getParam("ores_prob_to","1.0",true)).c_str() ) ;
	
	// Add misc
	db_params.redirects = getParam("show_redirects","both") ;
	db_params.combine = getParam("combination","subset") ;
	db_params.larger = atoi ( getParam("larger","-1",true).c_str() ) ;
	db_params.smaller = atoi ( getParam("smaller","-1",true).c_str() ) ;
	db_params.minlinks = atoi ( getParam("minlinks","-1",true).c_str() ) ;
	db_params.maxlinks = atoi ( getParam("maxlinks","-1",true).c_str() ) ;
	db_params.page_wikidata_item = getParam("wikidata_item","any") ;
	
	// Time/date range
	db_params.before = getParam("before","") ;
	db_params.after = getParam("after","") ;
	db_params.max_age = getParam("max_age","") ;
	if ( getParam("only_new" ,"") != "" ) db_params.only_new_since  = true ;
	

	// Add linked from
	splitParamIntoVector ( getParam("outlinks_yes","") , db_params.linked_from_all ) ;
	splitParamIntoVector ( getParam("outlinks_any","") , db_params.linked_from_any ) ;
	splitParamIntoVector ( getParam("outlinks_no" ,"") , db_params.linked_from_none  ) ;

	// Add links to
	splitParamIntoVector ( getParam("links_to_all","") , db_params.links_to_all ) ;
	splitParamIntoVector ( getParam("links_to_any","") , db_params.links_to_any ) ;
	splitParamIntoVector ( getParam("links_to_no" ,"") , db_params.links_to_none  ) ;
}


//____________________________________________________________________________________________________________

class TCombineSources ;

class TCombineSourcesPart {
public:
	string render () ;
	string join_mode = "intersect" ;
	string source ;
	TCombineSources *sub = NULL ;
} ;

class TCombineSources {
public:
	TCombineSources() {} ;
	uint32_t parse ( const vector <string> &v , uint32_t start = 0 ) ;
	bool run ( TPageList &pagelist , map <string,TSource *> &sources , TPlatform &platform ) ;
	string render () ;
	vector <TCombineSourcesPart> parts ;
	bool is_valid = true ;
} ;

uint32_t TCombineSources::parse ( const vector <string> &v , uint32_t start ) {
	uint32_t pos = start ;
	while ( pos < v.size() ) {
		if ( v[pos].empty() ) { pos++ ; continue ; }
		if ( v[pos] == ")" ) return pos ; // Sub-query done
		TCombineSourcesPart part ;
		if ( v[pos] == "and" ) { pos++; }
		else if ( v[pos] == "(" ) {
			part.sub = new TCombineSources ;
			pos = part.sub->parse ( v , pos+1 ) ;
			if ( !part.sub->is_valid ) { is_valid = false ; return pos ; }
			pos++ ;
			parts.push_back ( part ) ;
			continue ;
		}
		else if ( v[pos] == "or" ) { part.join_mode = "merge" ; pos++ ; }
		else if ( v[pos] == "not" ) { part.join_mode = "negate" ; pos++ ; }
		if ( pos >= v.size() ) { is_valid = false ; return pos ; }
		
		if ( v[pos] == "(" ) {
			part.sub = new TCombineSources ;
			pos = part.sub->parse ( v , pos+1 ) ;
			if ( !part.sub->is_valid ) { is_valid = false ; return pos ; }
		} else {
			part.source = v[pos] ;
		}
		pos++ ;
		
		parts.push_back ( part ) ;
	}
	return pos ;
}

string TCombineSources::render () {
	string ret ;
	for ( auto part:parts ) {
		ret += part.render() ;
	}
	return ret ;
}

bool TCombineSources::run ( TPageList &pagelist , map <string,TSource *> &sources , TPlatform &platform ) {
	for ( auto part:parts ) {
		if ( part.sub ) {
			TPageList tmp ( pagelist.wiki ) ;
			if ( !part.sub->run ( tmp , sources , platform ) ) {
				return false ;
			}
			pagelist.join ( part.join_mode , tmp ) ;
		} else {
			if ( sources.find(part.source) == sources.end() ) {
				platform.error ( "Source " + part.source + " not given" ) ;
				return false ;
			}
			pagelist.join ( part.join_mode , *sources[part.source] ) ;
		}
	}
	return true ;
}

string TCombineSourcesPart::render () {
	string ret = "" ;
	ret += join_mode + " " ;
	if ( sub ) {
		ret += "(" ;
		ret += sub->render() ;
		ret += ")" ;
	} else {
		ret += source ;
	}
	ret += " " ;
	return ret ;
}

void TPlatform::getCommonWikiAuto ( map <string,TSource *> &sources ) {
	map <string,string> wikis ;
	wikis["cats"] = getWiki() ;
	wikis["manual"] = getParam("manual_list_wiki","") ;
	wikis["wikidata"] = "wikidatawiki" ;
	wikis["labels"] = "wikidatawiki" ;
	
	string param = getParam("common_wiki","auto") ;

	if ( param.empty() || param == "auto" || wikis.find(param)==wikis.end() || wikis[param].empty() ) {
		map <string,bool> distinct_wikis ;
		for ( auto source:sources ) distinct_wikis[source.second->wiki] = true ;
		if ( distinct_wikis.size() == 1 ) {
			for ( auto dwi:distinct_wikis ) common_wiki = dwi.first ;
		} else {
			common_wiki = "wikidatawiki" ; // Multiple wikis, common denominator
		}
	} else {
		common_wiki = wikis[param] ;
	}

	if ( common_wiki.empty() ) return ; // Paranoia
	
	// Convert to common wiki
	for ( auto source:sources ) {
		source.second->convertToWiki ( common_wiki ) ;
	}
}

void TPlatform::combine ( TPageList &pagelist , map <string,TSource *> &sources ) {
	getCommonWikiAuto ( sources ) ;	

	// Get or create combination commands
	string source_combination = legacyCombinationParameters ( sources ) ;
	if ( source_combination.empty() ) {
		for ( auto source:sources ) {
			if ( !source_combination.empty() ) source_combination += " AND " ;
			source_combination += source.first ;
		}
		source_combination = getParam ( "source_combination" , source_combination , true ) ;
	}
	
	// Lexing
	std::transform(source_combination.begin(), source_combination.end(), source_combination.begin(), ::tolower);
	std::regex bopen("\\s*([\\(\\)])\\s*") ;
	string target ( " $1 " ) ;
	source_combination = std::regex_replace ( source_combination , bopen , target ) ;
	vector <string> parts ;
	split ( source_combination , parts , ' ' ) ;
//cout << "Lexing as '" << source_combination << "'\n" ;

	// Parsing
	TCombineSources cmb ;
	uint32_t last = cmb.parse ( parts ) ;
	if ( last != parts.size() ) cmb.is_valid = false ;
	
	if ( !cmb.is_valid ) {
		error ( "Could not parse '" + source_combination + "' properly" ) ;
	}
	
//cout << cmb.render() << endl ;

	// Run combination
	cmb.run ( pagelist , sources , *this ) ;
	wiki = pagelist.wiki ;
}

void TPlatform::sortResults ( TPageList &pagelist ) {
	// Sort pagelist
	bool asc = !(getParam("sortorder") == "descending") ;
	if ( getParam("sortby") == "title" ) pagelist.customSort ( PAGE_SORT_TITLE , asc ) ;
	else if ( getParam("sortby") == "ns_title" ) pagelist.customSort ( PAGE_SORT_NS_TITLE , asc ) ;
	else if ( getParam("sortby") == "size" ) pagelist.customSort ( PAGE_SORT_SIZE , asc ) ;
	else if ( getParam("sortby") == "date" ) pagelist.customSort ( PAGE_SORT_DATE , asc ) ;
	else if ( getParam("sortby") == "filesize" ) pagelist.customSort ( PAGE_SORT_FILE_SIZE , asc ) ;
	else if ( getParam("sortby") == "uploaddate" ) pagelist.customSort ( PAGE_SORT_UPLOAD_DATE , asc ) ;
	else if ( getParam("sortby") == "uploaddate" ) pagelist.customSort ( PAGE_SORT_UPLOAD_DATE , asc ) ;
	else if ( getParam("sortby") == "random" ) pagelist.customSort ( PAGE_SORT_RANDOM , asc ) ;
	else pagelist.customSort ( PAGE_SORT_DEFAULT , asc ) ;
}

string TPlatform::legacyCombinationParameters ( map <string,TSource *> &sources ) {
	string ret = getParam("source_combination","") ;
	if ( !ret.empty() ) return ret ;
	if ( params.find("mode_manual") == params.end() ) return ret ; // No autolist legacy parameters
	
	// Try to use old autolist parameters
	map <string,vector<string> > mode_sources ;
	if ( sources.find("manual") != sources.end() ) mode_sources[getParam("mode_manual","or",true)].push_back ( "manual" ) ;
	if ( sources.find("categories") != sources.end() ) mode_sources[getParam("mode_cat","or",true)].push_back ( "categories" ) ;
	// WDQ not supported
	if ( sources.find("sparql") != sources.end() ) mode_sources[getParam("mode_wdqs","or",true)].push_back ( "sparql" ) ;
	// Find not supported
	if ( sources.find("pagepile") != sources.end() ) mode_sources[getParam("c","or",true)].push_back ( "pagepile" ) ;
	if ( mode_sources.empty() ) return ret ; // Nope
	
	for ( auto source:mode_sources["or"] ) ret += ret.empty() ? source : " OR " + source ;
	if ( !ret.empty() ) ret = "(" + ret + ")" ;
	for ( auto source:mode_sources["and"] ) ret += ret.empty() ? source : " AND " + source ;
	if ( !ret.empty() ) ret = "(" + ret + ")" ;
	for ( auto source:mode_sources["not"] ) ret += ret.empty() ? source : " NOT " + source ;
	return ret ;
}

void TPlatform::legacyAutoListParameters () {
	if ( !getParam("max","").empty() ) { // QuickIntersection URL
		if ( getParam("format","") == "jsonfm" ) { params["json-pretty"] = 1 ; query += "&json-pretty=1" ; }
		if ( getParam("output_compatability","") != "quick-intersection" ) { params["output_compatability"] = "quick-intersection" ; query += "&output_compatability=quick-intersection" ; }
		params["ns["+getParam("ns")+"]"] = 1 ;
		query += "&ns["+getParam("ns")+"]=1" ;
	}
	if ( getParam("language","").empty() && !getParam("lang","").empty() ) {
		params["language"] = getParam("lang","") ;
		query += "&language=" + urlencode ( getParam("language","") ) ;
	}
	if ( getParam("language","").empty() && getParam("lang","").empty() && !getParam("interface_language","").empty() ) {
		params["language"] = getParam("interface_language","") ;
		query += "&language=" + urlencode ( getParam("interface_language","") ) ;
	}
	if ( getParam("categories","").empty() && !getParam("category","").empty() ) {
		params["categories"] = getParam("category","") ;
		query += "&categories=" + urlencode ( getParam("category","") ) ;
	}
	if ( getParam("categories","").empty() && !getParam("cats","").empty() ) {
		params["categories"] = getParam("cats","") ;
		query += "&categories=" + urlencode ( getParam("category","") ) ;
	}
	if ( getParam("sparql","").empty() && !getParam("wdqs","").empty() ) {
		params["sparql"] = getParam("wdqs","") ;
		query += "&sparql=" + urlencode ( getParam("wdqs","") ) ;
	}
	if ( getParam("sparql","").empty() && !getParam("wdq","").empty() ) {
		string wdq = getParam("wdq","") ;
		string url = "https://tools.wmflabs.org/wdq2sparql/w2s.php?wdq=" + urlencode(wdq) ;
		string text = loadTextfromURL ( url ) ;
		if ( !text.empty() && text.substr(0,9) != "<!DOCTYPE" ) {
			// TODO trim
			params["sparql"] = text ;
			query += "&sparql=" + urlencode ( text ) ;
		} else {
			errors.push_back ( "Could not automatically convert from WDQ to SPARQL. Your results will be different than in AutoList 2." ) ;
		}
	}
}

std::mutex source_mutex ;
void runSource ( TSource *source , map <string,TSource *> *sources ) {
	if ( source->run() ) {
		std::lock_guard<std::mutex> lock(source_mutex);
		(*sources)[source->getSourceName()] = source ;
	} else {
	}
}

string TPlatform::process () {
	struct timeval before , after;
	gettimeofday(&before , NULL);
	
	legacyAutoListParameters() ;

	// Potential sources
	vector <TSource *> candidate_sources ;
	candidate_sources.push_back ( new TSourceDatabase ( this ) ) ;
	candidate_sources.push_back ( new TSourceSPARQL ( this ) ) ;
	candidate_sources.push_back ( new TSourcePagePile ( this ) ) ;
	candidate_sources.push_back ( new TSourceManual ( this ) ) ;
	candidate_sources.push_back ( new TSourceWikidata ( this ) ) ;
	
	// Check and run sources
	map <string,TSource *> sources ;
	vector <std::thread*> threads ;
	for ( auto source:candidate_sources ) {
		threads.push_back ( new std::thread ( &runSource , source , &sources ) ) ;
	}
	
	if ( sources.size() == 0 ) { // Try fallbacks!
		// Reset previous
		for ( auto t:threads ) t->join() ;
		candidate_sources.clear() ;
		threads.clear() ;
		
		// Add fallback(s)
		label_filter_used_as_primary = true ;
		candidate_sources.push_back ( new TSourceLabels ( this ) ) ;
		for ( auto source:candidate_sources ) {
			threads.push_back ( new std::thread ( &runSource , source , &sources ) ) ;
		}
	}
	
	for ( auto t:threads ) t->join() ;

	// Combine sources
	TPageList pagelist ( getWiki() ) ;
	combine ( pagelist , sources ) ;

	// Filter and post-process
	filterWikidata ( pagelist ) ;
	processSitelinks ( pagelist ) ;
	processLabels ( pagelist ) ;
	if ( !common_wiki.empty() && pagelist.wiki != common_wiki ) pagelist.convertToWiki ( common_wiki ) ;
	if ( sources.find("categories") == sources.end() ) processMissingDatabaseFilters ( pagelist ) ;
	processWikidata ( pagelist ) ;
	processFiles ( pagelist ) ;
	processPages ( pagelist ) ;
	processSubpages ( pagelist ) ;

	gettimeofday(&after , NULL);
	querytime = time_diff(before , after)/1000000 ;

	string wikidata_label_language = getParam ( "wikidata_label_language" , "" ) ;
	if ( wikidata_label_language.empty() ) wikidata_label_language = getParam("interface_language","en") ;
	pagelist.loadMissingMetadata ( wikidata_label_language ) ;
	
	pagelist.regexpFilter ( getParam("regexp_filter","") ) ;
	
	
	sortResults ( pagelist ) ;
	processRedlinks ( pagelist ) ; // Supersedes sort
	params["format"] = getParam ( "format" , "html" , true ) ;

	processCreator ( pagelist ) ;

	TRenderer renderer ( this ) ;
	return renderer.renderPageList ( pagelist ) ;
}

// Runs page filters from the "Page properties" tab if (and only if) no category/template/links_from primary query has occurred
void TPlatform::processMissingDatabaseFilters ( TPageList &pagelist ) {
	if ( pagelist.pages.size() == 0 ) return ;
	TSourceDatabase db ( this ) ;
	db.setPageList ( &pagelist ) ;
	if ( db.run() ) {
		db.swap ( pagelist ) ;
	} else {
		error ( "processMissingDatabaseFilters fail" ) ;
	}
}

void TPlatform::getParameterAsStringArray ( string s , vector <string> &vs ) {
	vs.clear() ;
	stringReplace ( s  , "\r" , "" ) ;
	split ( trim(s) , vs , '\n' ) ;
}

string TPlatform::getLabelBaseSQL ( TWikidataDB &db ) {
	vector <string> yes , any , no ;
	getParameterAsStringArray ( getParam("labels_yes","") , yes ) ;
	getParameterAsStringArray ( getParam("labels_any","") , any ) ;
	getParameterAsStringArray ( getParam("labels_no","")  , no  ) ;
	if ( yes.size()+any.size()+no.size() == 0 ) return "" ; // No point in filtering...

	string langs_yes = getParam("langs_labels_yes","") ;
	string langs_any = getParam("langs_labels_any","") ;
	string langs_no  = getParam("langs_labels_no" ,"") ;
	langs_yes = trim ( regex_replace ( langs_yes , regex("[^a-z,]") , string("") ) ) ;
	langs_any = trim ( regex_replace ( langs_any , regex("[^a-z,]") , string("") ) ) ;
	langs_no  = trim ( regex_replace ( langs_no  , regex("[^a-z,]") , string("") ) ) ;
	
	string sql = "SELECT DISTINCT term_full_entity_id FROM wb_terms t1 where term_entity_type='item'" ;
	string field = "term_text" ; // term_search_key case-sensitive; term_text case-insensitive?
	
	if ( yes.size() > 0 ) {
		for ( auto s:yes ) {
			sql += " AND " + field + " LIKE \"" + db.escape(s) + "\"" ;
			if ( !langs_yes.empty() ) sql += " AND term_language IN ('" + regex_replace ( langs_yes , regex(",") , string("','") ) + "')" ;
			vector <string> types ;
			if ( !getParam("cb_labels_yes_l","").empty() ) types.push_back ( "label" ) ;
			if ( !getParam("cb_labels_yes_a","").empty() ) types.push_back ( "alias" ) ;
			if ( !getParam("cb_labels_yes_d","").empty() ) types.push_back ( "description" ) ;
			if ( types.size() > 0 ) sql += " AND term_type IN (" + TSourceDatabase::listEscapedStrings ( db , types , true ) + ")" ;
		}
	}

	if ( any.size() > 0 ) {
		sql += " AND (" ;
		bool first = true ;
		for ( auto s:any ) {
			if ( first ) first = false ;
			else sql += " OR " ;
			sql += " (" + field + " LIKE \"" + db.escape(s) + "\"" ;
			if ( !langs_any.empty() ) sql += " AND term_language IN ('" + regex_replace ( langs_any , regex(",") , string("','") ) + "')" ;
			vector <string> types ;
			if ( !getParam("cb_labels_any_l","").empty() ) types.push_back ( "label" ) ;
			if ( !getParam("cb_labels_any_a","").empty() ) types.push_back ( "alias" ) ;
			if ( !getParam("cb_labels_any_d","").empty() ) types.push_back ( "description" ) ;
			if ( types.size() > 0 ) sql += " AND term_type IN (" + TSourceDatabase::listEscapedStrings ( db , types , true ) + ")" ;
			sql += ")" ;
		}
		sql += ")" ;
	}
	
	if ( no.size() > 0 ) {
		for ( auto s:no ) {
			sql += " AND NOT EXISTS (SELECT t2.term_full_entity_id FROM wb_terms t2 WHERE" ;
			sql += " t2.term_full_entity_id=t1.term_full_entity_id AND t2.term_entity_type='item'" ;
			sql += " AND t2." + field + " LIKE \"" + db.escape(s) + "\"" ;
			if ( !langs_no.empty() ) sql += " AND t2.term_language IN ('" + regex_replace ( langs_no , regex(",") , string("','") ) + "')" ;
			vector <string> types ;
			if ( !getParam("cb_labels_no_l","").empty() ) types.push_back ( "label" ) ;
			if ( !getParam("cb_labels_no_a","").empty() ) types.push_back ( "alias" ) ;
			if ( !getParam("cb_labels_no_d","").empty() ) types.push_back ( "description" ) ;
			if ( types.size() > 0 ) sql += " AND t2.term_type IN (" + TSourceDatabase::listEscapedStrings ( db , types , true ) + ")" ;
			sql += ")" ;
		}
	}

	return sql ;
}
	
void TPlatform::processLabels ( TPageList &pagelist ) {
	if ( label_filter_used_as_primary ) return ; // Been there, done that...
	
	TWikidataDB db ( pagelist.wiki , this ) ;
	string sql = getLabelBaseSQL ( db ) ;
	if ( sql.empty() ) return ;
	pagelist.convertToWiki ( "wikidatawiki" ) ;
	if ( pagelist.pages.empty() ) return ; // No point in filtering...
	
	// Adding items; maybe do in batches?
	string items ;
	map <string,TPage> qnum2page ;
	for ( auto page:pagelist.pages ) {
		if ( !items.empty() ) items += "," ;
		items += "'" + page.name + "'" ;
		qnum2page[page.name] = page ;
	}
	pagelist.pages.clear() ;

	sql += " AND term_full_entity_id IN (" + items + ")" ;

	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		pagelist.pages.push_back ( qnum2page[row[0]] ) ;
	}
}

void TPlatform::processSitelinks ( TPageList &pagelist ) {
	string sitelinks_yes = getParam("sitelinks_yes","") ;
	string sitelinks_any = getParam("sitelinks_any","") ;
	string sitelinks_no = getParam("sitelinks_no","") ;
	string sitelinks_min = getParam("min_sitelink_count","") ;
	string sitelinks_max = getParam("max_sitelink_count","") ;

	if ( trim(sitelinks_min) == "0" ) sitelinks_min.clear() ;
	if ( sitelinks_yes.empty() && sitelinks_any.empty() && sitelinks_no.empty() && sitelinks_min.empty() && sitelinks_max.empty() ) return ;
	pagelist.convertToWiki ( "wikidatawiki" ) ;
	if ( pagelist.pages.empty() ) return ; // No point in filtering...
	
	stringReplace ( sitelinks_yes , "\r" , "" ) ;
	stringReplace ( sitelinks_any , "\r" , "" ) ;
	stringReplace ( sitelinks_no  , "\r" , "" ) ;

	vector <string> yes , any , no ;
	split ( trim(sitelinks_yes) , yes , '\n' ) ;
	split ( trim(sitelinks_any) , any , '\n' ) ;
	split ( trim(sitelinks_no)  , no  , '\n' ) ;
	bool use_min_max = !sitelinks_min.empty() || !sitelinks_max.empty() ;
	
	TWikidataDB db ( pagelist.wiki , this ) ;
	
	// Construct base SQL
	string sql = "SELECT " ;
	if ( use_min_max ) sql += "page_title,(SELECT count(*) FROM wb_items_per_site WHERE ips_item_id=substr(page_title,2)*1) AS sitelink_count" ;
	else sql += "DISTINCT page_title" ;
	sql += " FROM page WHERE page_namespace=0" ;
	string having ;

	for ( auto site:yes ) {
		if ( site.empty() ) continue ;
		sql += " AND EXISTS (SELECT * FROM wb_items_per_site WHERE ips_item_id=substr(page_title,2)*1 AND ips_site_id='" + db.escape(site) + "' LIMIT 1)" ;
	}
	if ( any.size() > 0 ) {
		sql += " AND EXISTS (SELECT * FROM wb_items_per_site WHERE ips_item_id=substr(page_title,2)*1 AND ips_site_id IN (" + TSourceDatabase::listEscapedStrings ( db , any , true ) + ") LIMIT 1)" ;
	}
	for ( auto site:no ) {
		if ( site.empty() ) continue ;
		sql += " AND NOT EXISTS (SELECT * FROM wb_items_per_site WHERE ips_item_id=substr(page_title,2)*1 AND ips_site_id='" + db.escape(site) + "' LIMIT 1)" ;
	}
	if ( !sitelinks_min.empty() ) {
		string s = ui2s ( atoi ( sitelinks_min.c_str() ) ) ; // Enforce number
		if ( !having.empty() ) having += " AND " ;
		having += "sitelink_count>=" + s ;
	}
	if ( !sitelinks_max.empty() ) {
		string s = ui2s ( atoi ( sitelinks_max.c_str() ) ) ; // Enforce number
		if ( !having.empty() ) having += " AND " ;
		having += "sitelink_count<=" + s ;
	}
	

	string items ;
	map <string,TPage> qnum2page ;
	for ( auto page:pagelist.pages ) {
		if ( !items.empty() ) items += "," ;
		string qnum = page.name ;
		items += "'" + qnum + "'" ;
		qnum2page[qnum] = page ;
	}
	pagelist.pages.clear() ;

	sql += " AND page_title IN (" + items + ")" ;
	
	if ( use_min_max ) sql += " GROUP BY page_title" ;
	if ( !having.empty() ) sql += " HAVING " + having ;

	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		pagelist.pages.push_back ( qnum2page[row[0]] ) ;
	}
	
}

void TPlatform::processRedlinks ( TPageList &pagelist ) {
	if ( pagelist.size() == 0 ) return ;
	if ( getParam("show_redlinks","").empty() ) return ;
	if ( wiki == "wikidiatawiki" ) {
		error ( "Redlinks are rather pointless on Wikidata" ) ;
		return ;
	}
	
	output_redlinks = true ;
	bool ns0_only = !(getParam("article_redlinks_only","").empty()) ;
	bool remove_template_redlinks = !(getParam("remove_template_redlinks","").empty()) ;

	map <pair<uint32_t,string>,uint32_t> redlinks ;
	TWikidataDB db ( pagelist.wiki , this ) ;
	uint32_t cnt = 0 ;
	map <int32_t,vector <string> > nslist ;
	auto p = pagelist.pages.begin() ;
	do {
		nslist[p->meta.ns].push_back ( p->name ) ;
		cnt++ ;
		p++ ;
		if ( cnt >= 200 /*DB_PAGE_BATCH_SIZE*/ || ( cnt > 0 && p == pagelist.pages.end() ) ) {
			for ( auto nsgroup:nslist ) {
				string sql = "SELECT pl_namespace,pl_title,(SELECT COUNT(*) FROM page p1 WHERE p1.page_title=pl0.pl_title AND p1.page_namespace=pl0.pl_namespace) AS cnt from page p0,pagelinks pl0 WHERE pl_from=p0.page_id" ;
				sql += " AND p0.page_namespace=" + ui2s(nsgroup.first) ;
				sql += " AND p0.page_title IN (" + TSourceDatabase::listEscapedStrings ( db , nsgroup.second ) + ")" ;
				if ( ns0_only ) sql += " AND pl_namespace=0" ;
				else sql += " AND pl_namespace>=0" ;
				
				if ( remove_template_redlinks ) {
					sql += " AND NOT EXISTS (SELECT * FROM pagelinks pl1 WHERE pl1.pl_from_namespace=10 AND pl0.pl_namespace=pl1.pl_namespace AND pl0.pl_title=pl1.pl_title LIMIT 1)" ;
				}
				sql += " GROUP BY page_id,pl_namespace,pl_title" ;
				sql += " HAVING cnt=0" ;

				MYSQL_RES *result = db.getQueryResults ( sql ) ;
				MYSQL_ROW row;
				while ((row = mysql_fetch_row(result))) {
					pair<int,string> ns_title ( atoi(row[0]) , string(row[1]) ) ;
					redlinks[ns_title]++ ;
				}
				mysql_free_result(result);
			}
			
			cnt = 0 ;
			nslist.clear() ;
		}
	} while ( p != pagelist.pages.end() ) ;
	
	uint32_t min_redlinks = atoi(getParam("min_redlink_count","1").c_str()) ;
	pagelist.pages.clear() ;
	for ( auto rl:redlinks ) {
		if ( rl.second < min_redlinks ) continue ;
		TPage redpage ( rl.first.second , rl.first.first ) ;
		redpage.meta.misc["count"] = ui2s(rl.second) ;
		pagelist.pages.push_back ( redpage ) ;
	}
	pagelist.customSort ( PAGE_SORT_REDLINKS_COUNT , false ) ;
}

void TPlatform::filterWikidata ( TPageList &pagelist ) {
	bool no_statements = !getParam("wpiu_no_statements","").empty() ;
	bool no_sitelinks = !getParam("wpiu_no_sitelinks","").empty() ;
	string wpiu = getParam ( "wpiu" , "any" ) ;
	if ( pagelist.size() == 0 ) return ;
	string list = trim ( getParam ( "wikidata_prop_item_use" , "" ) ) ;
	if ( list.empty() && !no_statements && !no_sitelinks ) return ;
	pagelist.convertToWiki ( "wikidatawiki" ) ;
	wiki = pagelist.wiki ;
	if ( pagelist.size() == 0 ) return ;
	
	
	vector <string> items , props , tmp ;
	std::transform(list.begin(), list.end(), list.begin(), ::toupper);
	split ( list , tmp , ',' ) ;
	for ( auto s:tmp ) {
		s = trim ( s ) ;
		if ( s.empty() ) continue ;
		if ( s[0] == 'P' ) props.push_back ( s ) ;
		if ( s[0] == 'Q' ) items.push_back ( s ) ;
	}



	// This method assumes a "small number" (<DB_PAGE_BATCH_SIZE), so won't do batching. Yes I'm lazy.

	map <string,TPage> title2page ;
	tmp.clear() ;
	tmp.reserve ( pagelist.size() ) ;
	for ( auto i: pagelist.pages ) {
		if ( i.meta.ns != 0 ) continue ;
		tmp.push_back ( i.name ) ;
		title2page[i.name] = i ;
	}
	
	TWikidataDB db ( "wikidatawiki" , this ) ;
	string sql = "SELECT DISTINCT page_title FROM page" ;
	if ( no_statements || no_sitelinks ) sql += ",page_props" ;
	sql += " WHERE page_namespace=0 AND page_title IN (" ;
	sql += TSourceDatabase::listEscapedStrings ( db , tmp , false ) ;
	sql += ")" ;
	
	
	// All/any/none
	if ( items.size()+props.size() > 0 ) {
	
		if ( wpiu == "any" ) sql += " AND (0 " ;
	
		if ( items.size() > 0 ) {
			if ( wpiu == "any" ) {
				sql += " OR EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=0 AND pl_title IN (" + TSourceDatabase::listEscapedStrings ( db , items , false ) + "))" ;
			} else if ( wpiu == "none" ) {
				sql += " AND NOT EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=0 AND pl_title IN (" + TSourceDatabase::listEscapedStrings ( db , items , false ) + "))" ;
			} else if ( wpiu == "all" ) {
				for ( auto s:items ) {
					sql += " AND EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=0 AND pl_title IN ('" + db.escape(s) + "'))" ;
				}
			}
		}

		if ( props.size() > 0 ) {
			if ( wpiu == "any" ) {
				sql += " OR EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=120 AND pl_title IN (" + TSourceDatabase::listEscapedStrings ( db , props , false ) + "))" ;
			} else if ( wpiu == "none" ) {
				sql += " AND NOT EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=120 AND pl_title IN (" + TSourceDatabase::listEscapedStrings ( db , props , false ) + "))" ;
			} else if ( wpiu == "all" ) {
				for ( auto s:props ) {
					sql += " AND EXISTS (SELECT * FROM pagelinks WHERE pl_from=page_id AND pl_namespace=120 AND pl_title IN ('" + db.escape(s) + "'))" ;
				}
			}
		}

		if ( wpiu == "any" ) sql += ")" ;
	}
	
	
	if ( no_statements ) {
		sql += " AND page_id=pp_page AND pp_propname='wb-claims' AND pp_sortkey=0" ;
	}
	if ( no_sitelinks ) {
		sql += " AND page_id=pp_page AND pp_propname='wb-sitelinks' AND pp_sortkey=0" ;
	}
	
	pagelist.pages.clear() ;
	
	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) {
		string q ( row[0] ) ;
		if ( title2page.find(q) == title2page.end() ) continue ;
		pagelist.pages.push_back ( title2page[q] ) ;
	}
	mysql_free_result(result);
}

void TPlatform::processCreator ( TPageList &pagelist ) {
	if ( pagelist.wiki == "wikidatawiki" ) return ;
	if ( getParam("wikidata_item","") != "without" && !doOutputRedlinks() ) return ;
	if ( pagelist.size() == 0 ) return ;

	// This method assumes a "small number" (<DB_PAGE_BATCH_SIZE), so won't do batching. Yes I'm lazy.
	
	vector <string> tmp ;
	tmp.reserve ( pagelist.size() ) ;
	for ( auto i: pagelist.pages ) tmp.push_back ( i.name ) ;
	
	TWikidataDB db ( "wikidatawiki" , this ) ;
	string sql = "SELECT DISTINCT term_text FROM wb_terms WHERE term_entity_type='item' AND term_type IN ('label','alias') AND term_text IN (" ;
	sql += _2space ( TSourceDatabase::listEscapedStrings ( db , tmp , false ) ) ;
	sql += ")" ;

	MYSQL_RES *result = db.getQueryResults ( sql ) ;
	MYSQL_ROW row;
	while ((row = mysql_fetch_row(result))) existing_labels[row[0]] = true ;
	mysql_free_result(result);
}

void TPlatform::processSubpages ( TPageList &pl ) {
	bool add_subpages = !getParam("add_subpages","").empty() ;
	string subpage_filter = getParam("subpage_filter","either") ;
	if ( !add_subpages && subpage_filter != "subpages" && subpage_filter != "no_subpages" ) return ;
	
	if ( add_subpages ) {
		TPageList nl ( pl.wiki ) ;
		TWikidataDB db ( pl.wiki , this ) ;
		for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
			nl.pages.push_back ( *i ) ; // Original page
			string sql = "SELECT page_title FROM page WHERE page_namespace=" + ui2s(i->meta.ns) ;
			sql += " AND page_title LIKE '" + db.escape(space2_(i->getNameWithoutNamespace())) + "/%'" ;
			MYSQL_RES *result = db.getQueryResults ( sql ) ;
			MYSQL_ROW row;
			while ((row = mysql_fetch_row(result))) {
				TPage p ( row[0] , i->meta.ns ) ;
				nl.pages.push_back ( p ) ;
			}
			mysql_free_result(result);
		}
		pl.swap ( nl ) ;
	}
	
	if ( subpage_filter != "subpages" && subpage_filter != "no_subpages" ) return ;

	TPageList nl ( pl.wiki ) ;
	bool keep_subpages = ( subpage_filter == "subpages" ) ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		bool has_slash = ( i->name.find("/") != string::npos ) ;
		if ( has_slash != keep_subpages ) continue ;
		nl.pages.push_back ( *i ) ;
	}
	pl.swap ( nl ) ;
}

void TPlatform::processPages ( TPageList &pl ) {
	bool add_coordinates = !getParam("add_coordinates","").empty() ;
	bool add_image = !getParam("add_image","").empty() ;
	bool add_defaultsort = !getParam("add_defaultsort","").empty() ;
	bool add_disambiguation = !getParam("add_disambiguation","").empty() ;
	bool add_incoming_links = ( getParam("sortby") == "incoming_links" ) ;
	if ( !add_coordinates && !add_image && !add_defaultsort && !add_disambiguation & !add_incoming_links ) return ; // Nothing to do
	
	uint32_t cnt = 0 ;
	TWikidataDB db ( pl.wiki , this ) ;
	map <uint32_t,vector <TPage *> > ns_pages ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		ns_pages[i->meta.ns].push_back ( &(*i) )  ;
		cnt++ ;
		if ( cnt < DB_PAGE_BATCH_SIZE ) continue ;
		annotatePage ( db , ns_pages , add_image , add_coordinates , add_defaultsort , add_disambiguation , add_incoming_links ) ;
		cnt = 0 ;
		ns_pages.clear() ;
	}
	annotatePage ( db , ns_pages , add_image , add_coordinates , add_defaultsort , add_disambiguation , add_incoming_links ) ;
	
}

void TPlatform::annotatePage ( TWikidataDB &db , map <uint32_t,vector <TPage *> > &ns_pages , bool add_image , bool add_coordinates , bool add_defaultsort , bool add_disambiguation , bool add_incoming_links ) {
	for ( auto ns_group:ns_pages ) {
		vector <string> titles ;
		map <string,TPage *> title2page ;
		titles.reserve ( ns_group.second.size() ) ;
		for ( auto page:ns_group.second ) {
			titles.push_back ( page->name ) ;
			title2page[page->name] = page ;
		}
		string sql = "SELECT page_title" ;
		if ( add_image ) sql += ",(SELECT pp_value FROM page_props WHERE pp_page=page_id AND pp_propname='page_image' LIMIT 1) AS image" ;
		if ( add_coordinates ) sql += ",(SELECT concat(gt_lat,',',gt_lon) FROM geo_tags WHERE gt_primary=1 AND gt_globe='earth' AND gt_page_id=page_id LIMIT 1) AS coord" ;
		if ( add_defaultsort ) sql += ",(SELECT pp_value FROM page_props WHERE pp_page=page_id AND pp_propname='defaultsort' LIMIT 1) AS defaultsort" ;
		if ( add_disambiguation ) sql += ",(SELECT pp_value FROM page_props WHERE pp_page=page_id AND pp_propname='disambiguation' LIMIT 1) AS disambiguation" ;
		if ( add_incoming_links ) sql += ",(SELECT count(*) FROM pagelinks WHERE pl_namespace=page_namespace AND pl_title=page_title AND pl_from_namespace=0) AS incoming_links" ;

		sql += " FROM page" ;
		sql += " WHERE page_namespace=" + ui2s(ns_group.first) ;
		sql += " AND page_title IN (" + TSourceDatabase::listEscapedStrings ( db , titles , false ) + ")" ;

		MYSQL_RES *result = db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			string title = row[0] ;
			if ( title2page.find(title) == title2page.end() ) continue ; // Paranoia
			string image , coordinates , defaultsort , disambiguation , incoming_links ;
			uint32_t pos = 0 ;

			if ( add_image ) {
				char *s = row[++pos] ;
				if ( s ) image = trim(s) ;
			}
			if ( add_coordinates ) {
				char *s = row[++pos] ;
				if ( s ) coordinates = trim(s) ;
			}
			if ( add_defaultsort ) {
				char *s = row[++pos] ;
				if ( s ) defaultsort = trim(s) ;
			}
			if ( add_disambiguation ) {
				char *s = row[++pos] ;
				if ( s ) disambiguation = trim(s) ;
			}
			if ( add_incoming_links ) {
				char *s = row[++pos] ;
				if ( s ) incoming_links = trim(s) ;
			}

			if ( !image.empty() ) title2page[title]->meta.misc["image"] = image ;
			if ( !defaultsort.empty() ) title2page[title]->meta.misc["defaultsort"] = defaultsort ;
			if ( !disambiguation.empty() ) title2page[title]->meta.misc["disambiguation"] = disambiguation ;
			if ( !incoming_links.empty() ) title2page[title]->meta.misc["incoming_links"] = incoming_links ;
			if ( !coordinates.empty() ) {
				vector <string> v ;
				split ( coordinates , v , ',' ) ;
				if ( v.size() == 2 ) {
					title2page[title]->meta.misc["latitude"] = v[0] ;
					title2page[title]->meta.misc["longitude"] = v[1] ;
				}
			}

		}
		mysql_free_result(result);

	}
}



void TPlatform::processFiles ( TPageList &pl ) {
	bool giu = !getParam("giu","").empty() ;
	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = giu || !getParam("file_usage_data","").empty() ;
	bool file_usage_data_ns0 = !getParam("file_usage_data_ns0","").empty() ;
	if ( getParam("sortby") == "filesize" || getParam("sortby") == "uploaddate" ) file_data = true ;
	if ( !file_data && !file_usage ) return ; // Nothing to do

	TWikidataDB db ( pl.wiki , this ) ;
	map <string,TPage *> name2f ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		if ( i->meta.ns != NS_FILE ) continue ; // Files only
		name2f[space2_(i->getNameWithoutNamespace())] = &(*i) ;
		if ( name2f.size() < DB_PAGE_BATCH_SIZE ) continue ;
		annotateFile ( db , name2f , file_data , file_usage , file_usage_data_ns0 ) ;
	}
	annotateFile ( db , name2f , file_data , file_usage , file_usage_data_ns0 ) ;
	
}

void TPlatform::annotateFile ( TWikidataDB &db , map <string,TPage *> &name2f , bool file_data , bool file_usage , bool file_usage_data_ns0 ) {
	if ( name2f.empty() ) return ;
	
	vector <string> tmp ;
	tmp.reserve ( name2f.size() ) ;
	for ( auto i = name2f.begin() ; i != name2f.end() ; i++ ) tmp.push_back ( i->first ) ;
	
	if ( file_usage ) {
		string sql = "SELECT gil_to,GROUP_CONCAT(gil_wiki,':',gil_page_namespace_id,':',gil_page_namespace,':',gil_page_title SEPARATOR '|') AS x FROM globalimagelinks WHERE gil_to IN (" ;
		sql += TSourceDatabase::listEscapedStrings ( db , tmp , false ) ;
		sql += ")" ;
		if ( file_usage_data_ns0 ) sql += " AND gil_page_namespace_id=0" ;
		sql += " GROUP BY gil_to" ;

		MYSQL_RES *result = db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			string filename = row[0] ;
			if ( name2f.find(filename) == name2f.end() ) {
				cerr << "TPlatform::annotateFile: File not found(1): " << filename << endl ;
				continue ;
			}
			name2f[filename]->meta.misc["gil"] = row[1] ; // This can throw a "incomplete UTF-8 string" in trenderer:516 for some reason...
		}
		mysql_free_result(result);
		
	}

	if ( file_data ) {
		string sql = "SELECT img_name" ;
		for ( auto i = file_data_keys.begin() ; i != file_data_keys.end() ; i++ ) sql += "," + (*i) ;
		sql += " FROM image WHERE img_name IN (" ;
		sql += TSourceDatabase::listEscapedStrings ( db , tmp , false ) ;
		sql += ")" ;

		MYSQL_RES *result = db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			string filename = row[0] ;
			if ( name2f.find(filename) == name2f.end() ) {
				cerr << "TPlatform::annotateFile: File not found(2): " << filename << endl ;
				continue ;
			}
			TPage *file = name2f[filename] ;
			for ( uint32_t i = 0 ; i < file_data_keys.size() ; i++ ) file->meta.misc[file_data_keys[i]] = row[i+1] ;
		}
		mysql_free_result(result);
	}
	
	name2f.clear() ;
}


void TPlatform::processWikidata ( TPageList &pl ) {
	if ( pl.wiki == "wikidatawiki" ) return ; // Well, they all do have an item...
	string wdi = getParam("wikidata_item","no") ;
	if ( wdi != "any" && wdi != "with" && wdi != "without" ) return ;

	uint32_t with_wikidata_item = 0 ;
	TWikidataDB db ( "wikidatawiki" , this ) ;
	map <string,TPage *> name2o ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		name2o[_2space(i->name)] = &(*i) ;
		if ( name2o.size() < DB_PAGE_BATCH_SIZE ) continue ;
		with_wikidata_item += pl.annotateWikidataItem ( db , pl.wiki , name2o ) ;
	}
	with_wikidata_item += pl.annotateWikidataItem ( db , pl.wiki , name2o ) ;
	
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

string TPlatform::getWiki () {
	if ( !wiki.empty() ) return wiki ;
	string l = getParam ( "language" , getParam ( "interface_language" , "en" ) ) ;
	string p = getParam ( "project" , "wikipedia" ) ;
	if ( l == "wikidata" || p == "wikidata" ) wiki = "wikidatawiki" ;
	else if ( l == "species" || l == "wikispecies" || p == "wikispecies" ) wiki = "specieswiki" ;
	else if ( l == "commons" || l == "meta" ) wiki = l+"wiki" ;
	else if ( p == "wikipedia" ) wiki = l+"wiki" ;
	else wiki = l+p ;
	return wiki ;
}

string TPlatform::getParam ( string key , string default_value , bool ignore_empty ) {
	auto i = params.find(key) ;
	if ( i != params.end() ) {
		if ( ignore_empty && i->second.empty() ) return default_value ;
		return i->second ;
	}
	return default_value ;
}

void TPlatform::setConfig ( TPlatform &p ) {
	config = p.config ;
}

bool TPlatform::readConfigFile ( string filename ) {
	ifstream in ( filename.c_str() ) ;
	json j ;
	try {
		in >> j ;
	} catch ( json::exception& e ) {
		cout << "message: " << e.what() << "\n" << "exception id: " << e.id << std::endl;
		return false ;
	}

	for ( auto i = j.begin() ; i != j.end() ; i++ ) {
		config[i.key()] = i.value() ;
	}
	
	return true ;
}

string TPlatform::getExistingLabel ( string name ) {
	string ret ;
	if ( existing_labels.find(_2space(name)) != existing_labels.end() ) ret = existing_labels[_2space(name)] ;
	return ret ;
}
