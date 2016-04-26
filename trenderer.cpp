#include "main.h"


string TRenderer::renderPageListWiki ( TPageList &pagelist ) {
	platform->content_type = "text/plain; charset=utf-8" ;

	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;

	string wdi = platform->getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	is_wikidata = pagelist.wiki == "wikidatawiki" ;

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
		string pre = i->meta.ns == 0 ? "" : ":" ; // This should really involve the namespace name...
		if ( is_wikidata ) {
			string label = i->meta.getMisc("label","") ;
			if ( label.empty() ) ret += "| [[" + pre + _2space(i->name) + "|]]" ;
			else ret += "| [[" + pre + i->name + "|"+label+"]]" ;
		} else {
			ret += "| [[" + pre + _2space(i->name) + "|]]" ;
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


string TRenderer::renderPageListCTSV ( TPageList &pagelist , string mode ) {
	if ( mode == "csv" ) platform->content_type = "text/csv; charset=utf-8" ;
	if ( mode == "tsv" ) platform->content_type = "text/tab-separated-values; charset=utf-8" ;

	string ret ;
	
	initializeColumns() ;
	
	bool first = true ;
	string sep ( mode=="csv"?",":"\t" ) ;
	for ( auto col:columns ) {
		if ( col == "checkbox" ) continue ; // Not suitable for TSC/CSV
		string out = col ;
		if ( mode == "csv" ) escapeCSV ( out ) ;
		if ( first ) first = false ;
		else ret += sep ;
		ret += out ;
	}
	ret += "\n" ;
	
	uint32_t cnt = 0 ;
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		cnt++ ;
		ret += getTableRowCTSV ( cnt , *i , pagelist , mode ) ;
	}
	return ret ;
}


string TRenderer::getTableRowCTSV ( uint32_t cnt , TPage &page , TPageList &pagelist , string &mode ) {
	char tmp[1000] ;
	string ret ;
	string sep ( mode=="csv"?",":"\t" ) ;

	bool first = true ;
	for ( auto col:columns ) {
	
		string out ;

		if ( col == "checkbox" ) {
			continue ;
		} else if ( col == "number" ) {
			sprintf ( tmp , "%d" , cnt ) ;
			out = tmp ;
		} else if ( col == "image" ) {
			string file = page.meta.ns == 6 ? page.getNameWithoutNamespace() : page.meta.getMisc("image","") ;
			if ( !file.empty() ) {
				string url = "https://" + getWikiServer ( wiki ) + "/wiki/File:" + urlencode(file) ;
				out = url ;
			}
		} else if ( col == "title" ) {
			out = page.name ;
		} else if ( col == "pageid" ) {
			out = ui2s ( page.meta.id ) ;
		}else if ( col == "namespace" ) {
			out = pagelist.getNamespaceString(page.meta.ns) ;
		} else if ( col == "linknumber" ) {
			out = string(page.meta.getMisc("count","?")) ;
		} else if ( col == "length" ) {
			out = ui2s ( page.meta.size ) ;
		} else if ( col == "touched" ) {
			out = string(page.meta.timestamp) ;
		} else if ( col == "wikidata" ) {
			if ( page.meta.q != UNKNOWN_WIKIDATA_ITEM ) {
				out = "Q" + ui2s ( page.meta.q ) ;
			}
		} else if ( col == "coordinates" ) {
			string lat = page.meta.getMisc("latitude","") ;
			string lon = page.meta.getMisc("longitude","") ;
			if ( !lat.empty() && !lon.empty() ) {
				out = lat + "/" + lon ;
			}
		} else if ( col == "fileusage" ) {
			out = page.meta.getMisc("gil","") ;
		} else { // File data etc.
			out = page.meta.getMisc(col,"") ;
		}
		
		if ( first ) first = false ;
		else ret += sep ;
		if ( mode == "csv" ) escapeCSV ( out ) ;
		ret += out ;
	}

	ret += "\n" ;
	return ret ;
}


void TRenderer::escapeCSV ( string &out ) {
	stringReplace ( out , "\\" , "\\\\") ;
	stringReplace ( out , "\"" , "\\\"" ) ;
	out = '"' + out + '"' ;
}

string TRenderer::renderPageListHTML ( TPageList &pagelist ) {
	platform->content_type = "text/html; charset=utf-8" ;
	gettimeofday(&now_ish , NULL);

	string ret ;
	ret += "<hr/>" ;
	ret += "<script>var output_wiki='"+pagelist.wiki+"';</script>\n" ;
	
	for ( auto a:platform->errors ) {
		ret += "<div class='alert alert-danger' role='alert'>" + a + "</div>" ;
	}

	// Wikidata edit box?
	use_autolist = false ;
	autolist_creator_mode = false ;
	if ( pagelist.wiki != "wikidatawiki" && platform->getParam("wikidata_item","") == "without" ) {
		ret += "<div id='autolist_box' mode='creator'></div>" ;
		use_autolist = true ;
		autolist_creator_mode = true ;
	} else if ( pagelist.wiki == "wikidatawiki" ) {
		ret += "<div id='autolist_box' mode='autolist'></div>" ;
		use_autolist = true ;
	}
	
	// Gallery?
	// Todo: meta.misc["image"]
	if ( only_files && !use_autolist ) {
		ret += "<div id='file_results' style='float:right' class='btn-group' data-toggle='buttons'>" ;
		ret += "<label class='btn btn-secondary active'><input type='radio' checked name='results_mode' value='titles' autocomplete='off' /><span class='l_show_titles'></span></label>" ;
		ret += "<label class='btn btn-secondary'><input type='radio' name='results_mode' value='thumbnails' checked autocomplete='off' /><span class='l_show_thumbnails'></span></label>" ;
		ret += "</div>" ;
	}
	
	// Todo: Coordinates?

	// Header
	char tmp[1000] ;
	sprintf ( tmp , "<h2><a name='results'></a><span id='num_results' num='%ld'></span></h2>" , pagelist.pages.size() ) ;
	ret += tmp ;
	if ( pagelist.pages.size() == 0 ) return ret ; // No need for empty table
	
	if ( pagelist.size() > MAX_HTML_RESULTS ) {
		ret += "<div class='alert alert-warning' style='clear:both'>Only the first " + ui2s(MAX_HTML_RESULTS) + " results are shown in HTML, so as to not crash your browser; other formats will have complete results.</div>" ;
		pagelist.pages.resize ( MAX_HTML_RESULTS ) ;
	}
	
	
	initializeColumns() ;

	// Results table
	ret += "<div style='clear:both;overflow:auto'>" ;
	ret += getTableHeaderHTML() ;
	ret += "<tbody>" ;
	uint32_t cnt = 0 ;
	for ( auto i = pagelist.pages.begin() ; i != pagelist.pages.end() ; i++ ) {
		cnt++ ;
		ret += getTableRowHTML ( cnt , *i , pagelist ) ;
	}
	ret += "</tbody></table></div>" ;
	
	// Footer
	sprintf ( tmp , "<div style='font-size:8pt' id='query_length' sec='%2.2f'></div>" , platform->getQueryTime() ) ;
	ret += tmp ;
	
	ret += "<script src='autolist.js'></script>" ;
	
	return ret ;
}

void TRenderer::initializeColumns() {
	is_wikidata = wiki == "wikidatawiki" ;
	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = !platform->getParam("file_usage_data","").empty() ;
	bool add_coordinates = !platform->getParam("add_coordinates","").empty() ;
	bool add_image = !platform->getParam("add_image","").empty() ;

	string wdi = platform->getParam("wikidata_item","no") ;
	bool show_wikidata_item = (wdi=="any"||wdi=="with") ;

	columns.clear() ;
	if ( use_autolist ) columns.push_back ( "checkbox" ) ;
	columns.push_back ( "number" ) ;
	if ( add_image ) columns.push_back ( "image" ) ;
	columns.push_back ( "title" ) ;
	if ( platform->doOutputRedlinks() ) {
		columns.push_back ( "namespace" ) ;
		columns.push_back ( "linknumber" ) ;
	} else {
		columns.push_back ( "pageid" ) ;
		columns.push_back ( "namespace" ) ;
		columns.push_back ( "length" ) ;
		columns.push_back ( "touched" ) ;
	}
	if ( show_wikidata_item ) columns.push_back ( "wikidata" ) ;
	if ( add_coordinates ) columns.push_back ( "coordinates" ) ;
	if ( file_data ) {
		for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) columns.push_back ( *k ) ;
	}
	if ( file_usage ) columns.push_back ( "fileusage" ) ;
}

string TRenderer::getTableRowHTML ( uint32_t cnt , TPage &page , TPageList &pagelist ) {
	char tmp[1000] ;
	string ret ;
	ret += "<tr>" ;

	for ( auto col:columns ) {

		if ( col == "checkbox" ) {
			string q ;
			string checked = "checked" ;
			if ( autolist_creator_mode ) {
				string el = platform->getExistingLabel ( page.name ) ;
				if ( !el.empty() ) checked = "" ; // No checkbox check if label/alias exists
				if ( page.name.find_first_of('(') != string::npos ) checked = "" ; // Names with "(" are unchecked by default
				sprintf ( tmp , "create_item_%d_%ld" , cnt , now_ish.tv_usec ) ; // Using microtime to get unique checkbox 
				q = tmp ;
			} else {
				q = page.name.substr(1) ;
			}
			ret += "<td><input type='checkbox' class='qcb' q='"+q+"' id='autolist_checkbox_"+q+"' "+checked+"></td>" ;
		} else if ( col == "number" ) {
			sprintf ( tmp , "<td class='num'>%d</td>" , cnt ) ;
			ret += tmp ;
		} else if ( col == "image" ) {
			ret += "<td>" ;
			string file = page.meta.ns == 6 ? page.getNameWithoutNamespace() : page.meta.getMisc("image","") ;
			if ( !file.empty() ) {
				string url = "https://" + getWikiServer ( wiki ) + "/wiki/File:" + urlencode(file) ;
				string src = "https://" + getWikiServer ( wiki ) + "/wiki/Special:Redirect/file/" + urlencode(file) + "?width="+thumnail_size ;
				ret += "<div class='card thumbcard'>" ;
				ret += "<a target='_blank' href='" + url + "'><img class='card-img thumbcard-img' src='" + src + "'/></a>" ;
				ret += "</div>" ;
			}
			ret += "</td>" ;
		} else if ( col == "title" ) {
			ret += "<td class='link_container'>" + getLink ( page ) ;
			if ( is_wikidata ) {
				string desc = page.meta.getMisc("description","") ;
				if ( !desc.empty() ) ret += "<div class='smaller'>" + desc + "</div>" ;
			}
			ret += "</td>" ;
		} else if ( col == "pageid" ) {
			sprintf ( tmp , "<td class='num'>%d</td>" , page.meta.id ) ;
			ret += tmp ;
		}else if ( col == "namespace" ) {
			string nsname = pagelist.getNamespaceString(page.meta.ns) ;
			if ( nsname.empty() ) nsname = "<span class='l_namespace_0'>Article</span>" ;
			ret += "<td>" + nsname + "</td>" ;
		} else if ( col == "linknumber" ) {
			ret += "<td class='num'>" + string(page.meta.getMisc("count","?")) + "</td>" ;
		} else if ( col == "length" ) {
			sprintf ( tmp , "<td class='num'>%d</td>" , page.meta.size ) ;
			ret += tmp ;
		} else if ( col == "touched" ) {
			ret += "<td class='num'>" + string(page.meta.timestamp) + "</td>" ;
		} else if ( col == "wikidata" ) {
			ret += "<td>" ;
			if ( page.meta.q != UNKNOWN_WIKIDATA_ITEM ) {
				sprintf ( tmp , "Q%d" , page.meta.q ) ;
				ret += "<a target='_blank' href='https://www.wikidata.org/wiki/" ;
				ret += tmp ;
				ret += "'>" ;
				ret += tmp ;
				ret += "</a>" ;
			}
			ret += "</td>" ;
		} else if ( col == "coordinates" ) {
			ret += "<td>" ;
			string lat = page.meta.getMisc("latitude","") ;
			string lon = page.meta.getMisc("longitude","") ;
			if ( !lat.empty() && !lon.empty() ) {
				string lang = platform->getParam("interface_language","en") ;
				string url = "https://tools.wmflabs.org/geohack/geohack.php?language="+lang+"&params=" ;
				url += lat[0]=='-'?lat.substr(1)+"_S_":lat+"_N_" ;
				url += lon[0]=='-'?lon.substr(1)+"_W_":lon+"_E_" ;
				url += "globe:earth" ;
				ret += "<a class='smaller' target='_blank' href='" + url + "'>" + lat + "/" + lon + "</a>" ;
			}
			ret += "</td>" ;
		} else if ( col == "fileusage" ) {
			string gil = page.meta.getMisc("gil","") ;
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
		} else { // File data etc.
			ret += "<td>" ;
			string t = page.meta.getMisc(col,"") ;
			if ( !t.empty() && col == "img_user_text" ) {
				ret += "<a target='_blank' href='https://commons.wikimedia.org/wiki/User:"+urlencode(space2_(t))+"'>" + t + "</a>" ;
			} else ret += t ;
			ret += "</td>" ;
		}
	}

	ret += "</tr>" ;
	return ret ;
}

string TRenderer::getTableHeaderHTML() {
	string ret = "<table class='table table-sm table-striped' id='main_table'>" ;
	ret += "<thead><tr>" ;
	for ( auto col:columns ) {
		if ( col == "checkbox" ) ret += "<th></th>" ;
		else if ( col == "number" ) ret += "<th class='num'>#</th>" ;
		else if ( col == "image" ) ret += "<th class='l_h_image'></th>" ;
		else if ( col == "title" ) ret += "<th class='text-nowrap l_h_title'></th>" ;
		else if ( col == "pageid" ) ret += "<th class='text-nowrap l_h_id'></th>" ;
		else if ( col == "namespace" ) ret += "<th class='text-nowrap l_h_namespace'></th>" ;
		else if ( col == "linknumber" ) ret += "<th class='l_link_number'></th>" ;
		else if ( col == "length" ) ret += "<th class='text-nowrap l_h_len'></th>" ;
		else if ( col == "touched" ) ret += "<th class='text-nowrap l_h_touched'></th>" ;
		else if ( col == "wikidata" ) ret += "<th class='l_h_wikidata'></th>" ;
		else if ( col == "coordinates" ) ret += "<th class='l_h_coordinates'></th>" ;
		else if ( col == "fileusage" ) ret += "<th class='l_file_usage_data'></th>" ;
		else { // File data etc.
			for ( auto k = file_data_keys.begin() ; k != file_data_keys.end() ; k++ ) {
				if ( *k == col ) ret += "<th class='l_h_"+col+"'></th>" ;
			}
		}
	}
	ret += "</tr></thead>" ;
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

	bool giu = !platform->getParam("giu","").empty() ;
	bool file_data = !platform->getParam("ext_image_data","").empty() ;
	bool file_usage = giu || !platform->getParam("file_usage_data","").empty() ;
	if ( giu ) sparse = false ;
	
	if ( !callback.empty() ) ret += callback + "(" ;
	if ( mode == "catscan" ) {
		sprintf ( tmp , "%f" , platform->getQueryTime() ) ;
		ret += json_object_open ;
		ret += "\"n\":\"result\"" + json_comma ;
		ret += "\"a\":" + json_object_open + "\"querytime_sec\":" + string(tmp) ;
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
				if ( file_usage && !i->meta.getMisc("gil","").empty() ) {
					json giu = {} ;
					string s = i->meta.getMisc("gil","") ;
					vector <string> v0 ;
					split ( s , v0 , '|' ) ;
					for ( auto gil:v0 ) {
						vector <string> v1 ;
						split ( gil , v1 , ':' ) ;
						if ( v1.size() != 4 ) continue ; // Paranoia
						json ngiu ;
						ngiu["wiki"] = v1[0] ;
						ngiu["page"] = v1[3] ;
						ngiu["ns"] = v1[1] ;
						giu.push_back ( ngiu ) ;
					}
					o["giu"] = giu ;
				}
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
	
	only_files = true ;
	for ( auto page:pagelist.pages ) {
		if ( page.meta.ns == 6 ) continue ;
		only_files = false ;
	}
	
	string format = platform->getParam ( "format" , "html" , true ) ;
	string ret ;
	
	platform->content_type = "text/html; charset=utf-8" ; // Default
	
	if ( format == "json" || format == "jsonfm" ) return renderPageListJSON ( pagelist ) ;
	else if ( format == "wiki" ) return renderPageListWiki ( pagelist ) ;
	else if ( format == "tsv" ) return renderPageListCTSV ( pagelist , format ) ;
	else if ( format == "csv" ) return renderPageListCTSV ( pagelist , format ) ;
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

