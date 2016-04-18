#include "main.h"


string TRenderer::renderPageListWiki ( TPageList &pagelist ) {
	platform->content_type = "text/plain; charset=utf-8" ;

	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;

	string wdi = platform->getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	bool is_wikidata = pagelist.wiki == "wikidatawiki" ;

	string ret ;
	ret += "== " + platform->getParam("combination") + " ==\n" ;

	if ( platform->query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
		ret += "[https://petscan.wmflabs.org/?" + platform->query + " Regenerate this table].\n\n" ;
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
		if ( is_wikidata ) {
			string label = i->meta.getMisc("label","") ;
			if ( label.empty() ) ret += "| [[:" + _2space(i->name) + "|]]" ;
			else ret += "| [[" + i->name + "|"+label+"]]" ;
		} else {
			ret += "| [[:" + _2space(i->name) + "|]]" ;
		}
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

string TRenderer::renderPageListPagePile ( TPageList &pagelist ) {
	platform->content_type = "text/html; charset=utf-8" ;
	string ret ;

	string url = "https://tools.wmflabs.org/pagepile/api.php" ;
	string params = "action=create_pile_with_data&wiki="+pagelist.wiki+"&data=" ;
	for ( auto page:pagelist.pages ) {
		params += urlencode(page.name) + "%09" + ui2s(page.meta.ns) + "%0A" ;
	}
	

	json j ;
	bool b = loadJSONfromPOST ( url , params , j ) ;
	
	if ( !b ) {
		platform->error ( "PagePile failure" ) ;
		return ret ;
	}
	
	int pid = j["pile"]["id"] ;
	string new_url = "https://tools.wmflabs.org/pagepile/api.php?action=get_data&id=" + ui2s(pid) ;
	ret = "<html><head><<meta http-equiv=\"refresh\" content=\"0; url="+new_url+"\" /></head></html>" ;
	return ret ;
}


string TRenderer::renderPageListTSV ( TPageList &pagelist ) {
	platform->content_type = "text/tab-separated-values; charset=utf-8" ;

	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;

	string wdi = platform->getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	bool is_wikidata = pagelist.wiki == "wikidatawiki" ;

	string ret ;
	ret += "Title" ;
	if ( is_wikidata ) ret += "\tLabel" ;
	ret += "\tPage ID\tNamespace\tSize (bytes)\tLast change" ;
	if ( show_wikidata_item ) ret += "\tWikidata" ;

	if ( file_data ) {
		for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) ret += "\t" + (*k) ;
	}
	
	ret += "\n" ;
	
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		ret += _2space(i->name) ;
		if ( is_wikidata )  ret += "\t" + i->meta.getMisc("label","") ;
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

	if ( platform->query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
//		ret += "\n\nhttps://petscan.wmflabs.org/?" + query ;
	}
	
	return ret ;
}

string TRenderer::renderPageListHTML ( TPageList &pagelist ) {
	platform->content_type = "text/html; charset=utf-8" ;

	bool is_wikidata = pagelist.wiki == "wikidatawiki" ;
	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;

	string wdi = platform->getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;
	
	string ret ;
	ret += "<hr/>" ;
	ret += "<script>var output_wiki='"+pagelist.wiki+"';</script>\n" ;
	
	for ( auto a:platform->errors ) {
		ret += "<div class='alert alert-danger' role='alert'>" + a + "</div>" ;
	}

	struct timeval now_ish ;
	gettimeofday(&now_ish , NULL);
	
	bool use_autolist = false ;
	bool autolist_creator_mode = false ;
	if ( pagelist.wiki != "wikidatawiki" && platform->getParam("wikidata_item","") == "without" ) {
		ret += "<div id='autolist_box' mode='creator'></div>" ;
		use_autolist = true ;
		autolist_creator_mode = true ;
	} else if ( pagelist.wiki == "wikidatawiki" ) {
		ret += "<div id='autolist_box' mode='autolist'></div>" ;
		use_autolist = true ;
	}
	
	if ( only_files && !use_autolist ) {
		ret += "<div id='file_results' style='float:right' class='btn-group' data-toggle='buttons'>" ;
		ret += "<label class='btn btn-secondary active'><input type='radio' checked name='results_mode' value='titles' autocomplete='off' /><span class='l_show_titles'></span></label>" ;
		ret += "<label class='btn btn-secondary'><input type='radio' name='results_mode' value='thumbnails' checked autocomplete='off' /><span class='l_show_thumbnails'></span></label>" ;
		ret += "</div>" ;
	}
	
	char tmp[1000] ;
	sprintf ( tmp , "<h2><a name='results'></a><span id='num_results' num='%ld'></span></h2>" , pagelist.pages.size() ) ;
	ret += tmp ;
	if ( pagelist.pages.size() == 0 ) return ret ; // No need for empty table
	
	if ( pagelist.size() > MAX_HTML_RESULTS ) {
		ret += "<div class='alert alert-warning'>Only the first " + ui2s(MAX_HTML_RESULTS) + " results are shown in HTML, so as to not crash your browser; other formats will have complete results.</div>" ;
		pagelist.pages.resize ( MAX_HTML_RESULTS ) ;
	}
	
	ret += "<div style='clear:both;overflow:auto'><table class='table table-sm table-striped' id='main_table'>" ;
	ret += "<thead><tr>" ;
	if ( use_autolist ) ret += "<th></th>" ; // Checkbox column
	ret += "<th class='num'>#</th><th class='text-nowrap l_h_title'></th>" ;
//	if ( is_wikidata ) ret += "<th class='text-nowrap l_h_wd_desc'></th>" ;
	if ( platform->doOutputRedlinks() ) ret += "<th class='text-nowrap l_h_namespace'></th><th class='l_link_number'></th>" ;
	else ret += "<th class='text-nowrap l_h_id'></th><th class='text-nowrap l_h_namespace'></th><th class='text-nowrap l_h_len'></th><th class='text-nowrap l_h_touched'></th>" ;
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
		
		if ( use_autolist ) {
			string q ;
			string checked = "checked" ;
			if ( autolist_creator_mode ) {
				string el = platform->getExistingLabel ( i->name ) ;
				if ( !el.empty() ) checked = "" ; // No checkbox check if label/alias exists
				if ( i->name.find_first_of('(') != string::npos ) checked = "" ; // Names with "(" are unchecked by default
				sprintf ( tmp , "create_item_%d_%ld" , cnt , now_ish.tv_usec ) ; // Using microtime to get unique checkbox 
				q = tmp ;
			} else {
				q = i->name.substr(1) ;
			}
			ret += "<td><input type='checkbox' class='qcb' q='"+q+"' id='autolist_checkbox_"+q+"' "+checked+"></td>" ;
		}
		
		sprintf ( tmp , "<td class='num'>%d</td>" , cnt ) ;
		ret += tmp ;
//		ret += "<td style='width:" + string(is_wikidata?"25":"100") + "%'>" + getLink ( *i ) ;
		ret += "<td style='width:100%'>" + getLink ( *i ) ;
		if ( is_wikidata ) {
			string desc = i->meta.getMisc("description","") ;
			if ( !desc.empty() ) ret += "<div class='smaller'>" + desc + "</div>" ;
		}
		ret += "</td>" ;
//		if ( is_wikidata ) ret += "<td>" + i->meta.getMisc("description","") + "</td>" ;

		if ( platform->doOutputRedlinks() ) {
			ret += "<td>"+nsname+"</td>" ; // Namespace name
			ret += "<td class='num'>" + string(i->meta.getMisc("count","?")) + "</td>" ; // Count
		} else {
			sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.id ) ; // ID
			ret += tmp ;
			ret += "<td>"+nsname+"</td>" ; // Namespace name
			sprintf ( tmp , "<td class='num'>%d</td>" , i->meta.size ) ; // Size
			ret += tmp ;
			ret += "<td class='num'>" + string(i->meta.timestamp) + "</td>" ; // TS
		}
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
		if ( file_usage ) {
			string gil = i->meta.getMisc("gil","") ;
			vector <string> pages ;
			split ( gil , pages , '|' ) ;
			ret += "<td>" ;
			for ( auto page: pages ) {
				vector <string> parts ;
				split ( page , parts , ':' , 4 ) ;
				if ( parts.size() != 4 ) {
					ret += "<div>" + page + "</div>" ;
					continue ;
				}
				string title = parts[3] ;
				if ( !parts[2].empty() ) title = parts[2] + ":" + title ;
				string server = getWikiServer ( parts[0] ) ;
				ret += "<div class='fileusage'>" + parts[0] + ":<a href='https://"+server+"/wiki/"+urlencode(title)+"' target='_blank'>" + _2space(title) + "</a></div>" ;
			}
			ret += "</td>" ;
			
//			ret += "<td>" + gil + "</td>" ;
		}
		
		ret += "</tr>" ;
	}
	ret += "</tbody>" ;
	ret += "</table></div>" ;
	
	sprintf ( tmp , "<div style='font-size:8pt' id='query_length' sec='%2.2f'></div>" , platform->getQueryTime() ) ;
	ret += tmp ;
	
	return ret ;
}

string TRenderer::renderPageListJSON ( TPageList &pagelist ) {
	string ret ;
	string mode = platform->getParam("output_compatability","catscan") ;
	string callback = platform->getParam("callback","") ;
	bool sparse = !platform->getParam("sparse","").empty() ;
	bool pretty = !platform->getParam("json-pretty","").empty() ;
	char tmp[100] ;

	string json_array_open = "[" ;
	string json_array_close = "]" ;
	string json_object_open = "{" ;
	string json_object_close = "}" ;
	string json_comma = "," ;

	platform->content_type = "application/json; charset=utf-8" ;
	if ( pretty ) {
		platform->content_type = "text/html; charset=utf-8" ;
		ret += "<!DOCTYPE html>\n<html><meta><meta charset='utf-8'/></meta><body><pre>" ;
		json_array_open = "[\n<div style='margin-left:20px'>" ;
		json_array_close = "</div>]" ;
		json_object_open = "{\n<div style='margin-left:20px'>" ;
		json_object_close = "</div>}" ;
		json_comma = ",\n" ;
	}

	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;
	
	if ( !callback.empty() ) ret += callback + "(" ;
	if ( mode == "catscan" ) {
		sprintf ( tmp , "%f" , platform->getQueryTime() ) ;
		ret += json_object_open ;
		ret += "\"n\":\"result\"" + json_comma ;
		ret += "\"a\":" + json_object_open + "\"platform->getQueryTime()_sec\":" + string(tmp) ;
		if ( platform->query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
			json url ( "https://petscan.wmflabs.org/?" + platform->query ) ;
			ret += json_comma + "\"query\":" + url.dump() ;
			
		}
		ret += json_object_close + json_comma ;
		ret += "\"*\":" + json_array_open + json_object_open ;
		ret += "\"n\":\"combination\"" + json_comma ;
		ret += "\"a\":" + json_object_open ;
		ret += "\"type\":\"" + platform->getParam("combination","subset") + "\"" + json_comma ;
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
//		bool group_by_key = !platform->getParam("group_by_key","").empty() ; // Not supported
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
		if ( platform->query.length() < MAX_QUERY_OUTPUT_LENGTH ) {
			json url ( "https://petscan.wmflabs.org/?" + platform->query ) ;
			ret += "\"query\":" + url.dump() + json_comma ;
		}
		sprintf ( tmp , "%f" , platform->getQueryTime() ) ;
		ret += "\"start\":\"0\"" + json_comma ;
		ret += "\"max\":\"" + ui2s(pagelist.size()+1) + "\"" + json_comma ;
		ret += "\"platform->getQueryTime()\":\"" + string(tmp) + "s\"" + json_comma ;
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

string TRenderer::renderPageList ( TPageList &pagelist ) {
	wiki = pagelist.wiki ;
	
	string format = platform->getParam ( "format" , "html" , true ) ;
	string ret ;
	
	platform->content_type = "text/html; charset=utf-8" ; // Default
	
	if ( format == "json" ) return renderPageListJSON ( pagelist ) ;
	else if ( format == "wiki" ) return renderPageListWiki ( pagelist ) ;
	else if ( format == "tsv" ) return renderPageListTSV ( pagelist ) ;
	else if ( format == "pagepile" ) return renderPageListPagePile ( pagelist ) ;
	else return renderPageListHTML ( pagelist ) ;
	
	return "" ;
}

string TRenderer::getLink ( TPage &page ) {
	string label = page.getNameWithoutNamespace() ;
	string url = page.name ;
	std::replace ( label.begin(), label.end(), '_', ' ') ;
	std::replace ( url.begin(), url.end(), ' ', '_') ;
	url = urlencode ( url ) ;

	string ret = label ;
	if ( wiki == "wikidatawiki" && page.meta.misc.find("label") != page.meta.misc.end() ) {
		url = "https://" + getWikiServer ( wiki ) + "/wiki/" + url ;
		ret = "<a class='pagelink' target='_blank' href='" + url + "'>" + page.meta.misc["label"] + "</a>" ;
		ret += " <small><tt>[" + label + "]</tt></small>" ;
	} else {
		url = "https://" + getWikiServer ( wiki ) + "/wiki/" + url ;
		ret = "<a class='pagelink' target='_blank' href='" + url + "'>" + label + "</a>" ;
	}
	return ret ;
}

