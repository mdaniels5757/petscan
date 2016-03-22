#include "main.h"
#include <fstream>

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

	// Last edit
	db_params.last_edit_bot = getParam("edits[bots]","both") ;
	db_params.last_edit_anon = getParam("edits[anons]","both") ;
	db_params.last_edit_flagged = getParam("edits[flagged]","both") ;
	
	// Add misc
	db_params.redirects = getParam("show_redirects","both") ;
	db_params.combine = getParam("combination","subset") ;
	db_params.larger = atoi ( getParam("larger","-1",true).c_str() ) ;
	db_params.smaller = atoi ( getParam("smaller","-1",true).c_str() ) ;
	db_params.minlinks = atoi ( getParam("minlinks","-1",true).c_str() ) ;
	db_params.maxlinks = atoi ( getParam("maxlinks","-1",true).c_str() ) ;

	// Time/date range
	db_params.before = getParam("before","") ;
	db_params.after = getParam("after","") ;
	db_params.max_age = getParam("max_age","") ;
	if ( getParam("only_new" ,"") != "" ) db_params.only_new_since  = true ;
	

	// Add linked from
	splitParamIntoVector ( getParam("outlinks_yes","") , db_params.linked_from_all ) ;
	splitParamIntoVector ( getParam("outlinks_any","") , db_params.linked_from_any ) ;
	splitParamIntoVector ( getParam("outlinks_no" ,"") , db_params.linked_from_none  ) ;
	
	
	TPageList pagelist ( getWiki() ) ;
	
	if ( db.getPages ( db_params ) ) {
		pagelist.swap ( db ) ;
	}


	processWikidata ( pagelist ) ;
	processFiles ( pagelist ) ;


	// Sort pagelist
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


void TPlatform::processFiles ( TPageList &pl ) {
	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = !getParam("file_usage_data","").empty() ;
	if ( !file_data && !file_usage ) return ; // Nothing to do
	
	TWikidataDB db ( *this , pl.wiki ) ;
	map <string,TPage *> name2f ;
	for ( auto i = pl.pages.begin() ; i != pl.pages.end() ; i++ ) {
		if ( i->meta.ns != NS_FILE ) continue ; // Files only
		name2f[space2_(i->getNameWithoutNamespace())] = &(*i) ;
		if ( name2f.size() < DB_PAGE_BATCH_SIZE ) continue ;
		annotateFile ( db , name2f , file_data , file_usage ) ;
	}
	annotateFile ( db , name2f , file_data , file_usage ) ;
	
}

void TPlatform::annotateFile ( TWikidataDB &db , map <string,TPage *> &name2f , bool file_data , bool file_usage ) {
	if ( name2f.empty() ) return ;
//	cout << "Annotating " << name2f.size() << " files\n" ;
	
	vector <string> tmp ;
	tmp.reserve ( name2f.size() ) ;
	for ( auto i = name2f.begin() ; i != name2f.end() ; i++ ) tmp.push_back ( i->first ) ;
	
	if ( file_usage ) {
		string sql = "SELECT gil_to,GROUP_CONCAT(gil_wiki,':',gil_page_namespace_id,':',gil_page_namespace,':',gil_page_title SEPARATOR '|') AS x FROM globalimagelinks WHERE gil_to IN (" ;
		sql += TSourceDatabase::listEscapedStrings ( db , tmp , false ) ;
		sql += ") GROUP BY gil_to" ;

		MYSQL_RES *result = db.getQueryResults ( sql ) ;
		MYSQL_ROW row;
		while ((row = mysql_fetch_row(result))) {
			string filename = row[0] ;
			if ( name2f.find(filename) == name2f.end() ) {
				cerr << "TPlatform::annotateFile: File not found(1): " << filename << endl ;
				continue ;
			}
			name2f[filename]->meta.misc["gil"] = row[1] ;
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
	if ( l == "wikispecies" || p == "wikispecies" ) return "specieswiki" ;
	if ( l == "commons" || l == "meta" ) return l+"wiki" ;
	if ( p == "wikipedia" ) return l+"wiki" ;
	return l+p ;
}

string TPlatform::getParam ( string key , string default_value , bool ignore_empty ) {
	auto i = params.find(key) ;
	if ( i != params.end() ) {
		if ( ignore_empty && i->second.empty() ) return default_value ;
		return i->second ;
	}
	return default_value ;
}

string TPlatform::renderPageListWiki ( TPageList &pagelist ) {
	content_type = "text/plain; charset=utf-8" ;

	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = !getParam("file_usage_data","").empty() ;

	string wdi = getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	string ret ;
	ret += "== " + getParam("combination") + " ==\n" ;

	if ( query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
		ret += "[https://petscan.wmflabs.org/?" + query + " Regenerate this table].\n\n" ;
	}
	
	ret += "{| border=1 class='wikitable'\n" ;
	ret += "!Title !! Page ID !! Namespace !! Size (bytes) !! Last change" ;
	if ( show_wikidata_item ) ret += " !! Wikidata" ;

	if ( file_data ) {
		for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) ret += " !! " + (*k) ;
	}
	
	ret += "\n" ;
	
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		ret += "|-\n" ;
		ret += "| [[:" + _2space(i->name) + "|]]" ;
		ret += " || " + ui2s(i->meta.id) ;
		ret += " || " + ui2s(i->meta.ns) ;
		ret += " || " + ui2s(i->meta.size) ;
		ret += " || " + i->meta.timestamp ;

		if ( show_wikidata_item ) {
			ret += " ||" ;
			if ( i->meta.q != UNKNOWN_WIKIDATA_ITEM ) ret += "[[:d:Q" + ui2s(i->meta.q) + "|]]" ;
		}

		if ( file_data ) {
			for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) {
				ret += " || "  ;
				string v = i->meta.getMisc(*k,"") ;
				if ( *k == "img_user_text" && !v.empty() ) ret += "[[User:" + v + "|]]" ;
				else ret += i->meta.getMisc(*k,"") ;
			}
		}
//		if ( file_usage ) ret += json_comma + "\"gil\":\"" + ...

		ret += "\n" ;
	}

	ret += "|}\n" ;
	return ret ;
}

string TPlatform::renderPageListTSV ( TPageList &pagelist ) {
	content_type = "text/tab-separated-values; charset=utf-8" ;

	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = !getParam("file_usage_data","").empty() ;

	string wdi = getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	string ret ;
	ret += "Title\tPage ID\tNamespace\tSize (bytes)\tLast change" ;
	if ( show_wikidata_item ) ret += "\tWikidata" ;

	if ( file_data ) {
		for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) ret += "\t" + (*k) ;
	}
	
	ret += "\n" ;
	
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		ret += _2space(i->name) ;
		ret += "\t" + ui2s(i->meta.id) ;
		ret += "\t" + ui2s(i->meta.ns) ;
		ret += "\t" + ui2s(i->meta.size) ;
		ret += "\t" + i->meta.timestamp ;

		if ( show_wikidata_item ) {
			ret += "\t" ;
			if ( i->meta.q != UNKNOWN_WIKIDATA_ITEM ) ret += "Q" + ui2s(i->meta.q) ;
		}

		if ( file_data ) {
			for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) {
				ret += "\t" + i->meta.getMisc(*k,"") ;
			}
		}
//		if ( file_usage ) ret += json_comma + "\"gil\":\"" + ...

		ret += "\n" ;
	}

	if ( query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
//		ret += "\n\nhttps://petscan.wmflabs.org/?" + query ;
	}
	
	return ret ;
}

string TPlatform::renderPageListHTML ( TPageList &pagelist ) {
	content_type = "text/html; charset=utf-8" ;

	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = !getParam("file_usage_data","").empty() ;

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
	if ( file_data ) {
		for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) ret += "<th class='l_h_"+(*k)+"'></th>" ;
	}
	if ( file_usage ) ret += "<th class='l_file_usage_data'></th>" ;
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
		
		// File metadata
		if ( file_data ) {
			for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) {
				ret += "<td>" ;
				string t = i->meta.getMisc(*k,"") ;
				if ( !t.empty() && *k == "img_user_text" ) {
					ret += "<a target='_blank' href='https://commons.wikimedia.org/wiki/User:"+urlencode(space2_(t))+"'>" + t + "</a>" ;
				} else ret += t ;
				ret += "</td>" ;
			}
		}
		if ( file_usage ) ret += "<td>" + i->meta.getMisc("gil","") + "</td>" ;
		
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
	string mode = getParam("output_compatability","catscan") ;
	string callback = getParam("callback","") ;
	bool sparse = !getParam("sparse","").empty() ;
	bool pretty = !getParam("json-pretty","").empty() ;
	char tmp[100] ;

	string json_array_open = "[" ;
	string json_array_close = "]" ;
	string json_object_open = "{" ;
	string json_object_close = "}" ;
	string json_comma = "," ;

	content_type = "application/json; charset=utf-8" ;
	if ( pretty ) {
		content_type = "text/html; charset=utf-8" ;
		ret += "<!DOCTYPE html>\n<html><meta><meta charset='utf-8'/></meta><body><pre>" ;
		json_array_open = "[\n<div style='margin-left:20px'>" ;
		json_array_close = "</div>]" ;
		json_object_open = "{\n<div style='margin-left:20px'>" ;
		json_object_close = "</div>}" ;
		json_comma = ",\n" ;
	}

	bool file_data = !getParam("ext_image_data","").empty() ;
	bool file_usage = !getParam("file_usage_data","").empty() ;
	
	if ( !callback.empty() ) ret += callback + "(" ;
	if ( mode == "catscan" ) {
		sprintf ( tmp , "%f" , querytime ) ;
		ret += json_object_open ;
		ret += "\"n\":\"result\"" + json_comma ;
		ret += "\"a\":" + json_object_open + "\"querytime_sec\":" + string(tmp) ;
		if ( query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
			json url ( "https://petscan.wmflabs.org/?" + query ) ;
			ret += json_comma + "\"query\":" + url.dump() ;
			
		}
		ret += json_object_close + json_comma ;
		ret += "\"*\":" + json_array_open + json_object_open ;
		ret += "\"n\":\"combination\"" + json_comma ;
		ret += "\"a\":" + json_object_open ;
		ret += "\"type\":\"" + getParam("combination","subset") + "\"" + json_comma ;
		ret += "\"*\":"+json_array_open ;
		for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
			if ( i != pagelist.pages.begin() ) ret += json_comma ;
			if ( sparse ) {
				json name ( i->name ) ;
				ret += name.dump() ;
			} else {
				json o = {
					{"n","page"},
					{"title",i->getNameWithoutNamespace()},
					{"id",i->meta.id},
					{"namespace",i->meta.ns},
					{"len",i->meta.size},
					{"touched",i->meta.timestamp},
					{"nstext",pagelist.getNamespaceString(i->meta.ns)}
				} ;

				if ( i->meta.q != UNKNOWN_WIKIDATA_ITEM ) o["q"] = "Q" + ui2s(i->meta.q) ;
				if ( file_usage ) o["gil"] = i->meta.getMisc("gil","") ;
				if ( file_data ) {
					for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) {
						o[*k] = i->meta.getMisc(*k,"") ;
					}
				}
				
				ret += o.dump();
			}
		}
		ret += json_array_close ;
		ret += json_object_close ;
		ret += json_object_close + json_array_close ;
		ret += json_object_close ;
	} else if ( mode == "quick-intersection" ) {
//		bool group_by_key = !getParam("group_by_key","").empty() ; // Not supported
		string dummy = pagelist.getNamespaceString(0); // Dummy to force namespace loading
		ret += json_object_open ;
		ret += "\"namespaces\":"+json_object_open ;
		for ( auto i = pagelist.ns_local.begin() ; i != pagelist.ns_local.end() ; i++ ) {
			if ( i != pagelist.ns_local.begin() ) ret += json_comma ;
			sprintf ( tmp , "\"%d\":" , i->first ) ;
			ret += tmp ;
			json j ( i->second ) ;
			ret += j.dump() ;
		}
		ret += json_object_close ;
		ret += ",\"status\":\"OK\"" + json_comma ;
		if ( query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
			json url ( "https://petscan.wmflabs.org/?" + query ) ;
			ret += "\"query\":" + url.dump() + json_comma ;
		}
		sprintf ( tmp , "%f" , querytime ) ;
		ret += "\"start\":\"0\"" + json_comma ;
		ret += "\"max\":\"" + ui2s(pagelist.size()+1) + "\"" + json_comma ;
		ret += "\"querytime\":\"" + string(tmp) + "s\"" + json_comma ;
		ret += "\"pagecount\":\"" + ui2s(pagelist.size()) + "\"" + json_comma ;
		ret += "\"pages\":" ;
		ret += json_array_open ;

		for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
			if ( i != pagelist.pages.begin() ) ret += json_comma ;
			if ( sparse ) {
				json name ( i->name ) ;
				ret += name.dump() ;
			} else {
				json o = {
					{"page_id",i->meta.id},
					{"page_namespace",i->meta.ns},
					{"page_title",i->getNameWithoutNamespace()},
					{"page_latest",i->meta.timestamp},
					{"page_len",i->meta.size}
				} ;
				ret += o.dump() ;
			}
		}
		
		ret += json_array_close ;
		ret += json_object_close ;
	}
	
	
	if ( pretty ) ret += "</pre></body></html>" ;
	if ( !callback.empty() ) ret += ")" ;
	
	return ret ;
}

string TPlatform::renderPageList ( TPageList &pagelist ) {
	string format = getParam ( "format" , "html" ) ;
	string ret ;
	
	content_type = "text/html; charset=utf-8" ; // Default
	
	if ( format == "json" ) return renderPageListJSON ( pagelist ) ;
	else if ( format == "wiki" ) return renderPageListWiki ( pagelist ) ;
	else if ( format == "tsv" ) return renderPageListTSV ( pagelist ) ;
	else return renderPageListHTML ( pagelist ) ;
	
	return "" ;
}

string TPlatform::getLink ( TPage &page ) {
	string label = page.getNameWithoutNamespace() ;
	string url = page.name ;
	std::replace ( label.begin(), label.end(), '_', ' ') ;
	std::replace ( url.begin(), url.end(), ' ', '_') ;
	// TODO escape '
//	url = urlencode ( url ) ;
	url = "https://" + getWikiServer ( getWiki() ) + "/wiki/" + url ;
	return "<a target='_blank' href='" + url + "'>" + label + "</a>" ;
}

bool TPlatform::readConfigFile ( string filename ) {
	ifstream in ( filename.c_str() ) ;
	json j ( in ) ;
	for ( auto i = j.begin() ; i != j.end() ; i++ ) config[i.key()] = i.value() ;
	return true ;
}
