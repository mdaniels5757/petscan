var params = {} ;
var default_params = {
	'show_redirects':'both',
	'edits[bots]':'both',
	'edits[anons]':'both',
	'edits[flagged]':'both',
	'combination':'subset',
	'format':'html',
	'sortby':'none',
	'depth':'0',
	'wikidata_item':'no',
	'output_compatability':'catscan',
	'active_tab':'tab_categories',
	'common_wiki':'cats',
	'sortorder':'ascending'
} ;

var max_namespace_id = 0 ;
var namespaces_selected = [] ;
var namespaces_loading = false ;
var last_namespaces = {} ;
var last_namespace_project = '' ;
var interface_language = '' ;
var interface_text = {} ;

function deXSS ( s ) {
	return s.replace ( /<script/ , '' ) ; // TODO this should be better...
}

function getUrlVars () {
	var vars = {} ;
	var params = decodeURIComponent ( $('#querystring').text() ) ;
	if ( params == '' ) params = window.location.href.slice(window.location.href.indexOf('?') + 1) ;
	var hashes = params.split('&');
	if ( hashes.length >0 && hashes[0] == window.location.href ) hashes.shift() ;
	$.each ( hashes , function ( i , j ) {
		var hash = j.split('=');
		hash[1] += '' ;
		vars[decodeURIComponent(hash[0])] = decodeURIComponent(hash[1]).replace(/_/g,' ');
	} ) ;
	return vars;
}

function _t ( k , alt_lang ) {
	if ( typeof alt_lang == 'undefined' ) alt_lang = '' ;
	var l = interface_language ;
	if ( alt_lang != '' ) l = alt_lang ;
	var ret = "<i>" + k + "</i>" ;
	if ( typeof interface_text['en'][k] != 'undefined' ) ret = interface_text['en'][k] ;
	if ( typeof interface_text[l] != 'undefined' && typeof interface_text[l][k] != 'undefined' ) ret = interface_text[l][k] ;
	return ret ;
}

function setPermalink ( q ) {
	var psid = 0 ;
	$('span[name="psid"]').each ( function () { psid = $(this).text() ; } ) ;

	q = q.replace ( /&{0,1}doit=[^&]*&{0,1}/ , '&' ) ; // Removing auto-run
	
	// Removing empty parameters
	var lq ;
	do {
		lq = q ;
		q = q.replace ( /&[a-z_]+=&/g , '&' ) ;
	} while ( lq != q ) ;
	
	// Removing default values
	$.each ( default_params , function ( k , v ) {
		var key = '&{0,1}' + encodeURIComponent(k) + '=' + encodeURIComponent(v) + '&{0,1}' ;
		var r = new RegExp ( key ) ;
		q = q.replace ( r , '&' ) ;
	} ) ;
	
	var url = '/?' + q ;
	var h = _t("query_url") ;
	h = h.replace ( /\$1/ , url+"&doit=" ) ;
	h = h.replace ( /\$2/ , url ) ;
	
	if ( psid != 0 ) h += ' ' + (_t("psid_note")).replace ( /\$1/ , psid ) ;
	
	$('#permalink').html ( h ) ;
}

function applyParameters () {
	
	namespaces_selected = [] ;
	
	$.each ( params , function ( name , value ) {
		
		var m = name.match ( /^ns\[(\d+)\]$/ ) ;
		if ( m != null ) {
			namespaces_selected.push ( m[1]*1 ) ;
			return ;
		}
		
		$('input:radio[name="'+name+'"][value="'+value+'"]').prop('checked', true);
		
		$('input[type="text"][name="'+name+'"]').val ( deXSS(value) ) ;
		$('input[type="number"][name="'+name+'"]').val ( parseInt(value) ) ;
		$('textarea[name="'+name+'"]').val ( deXSS(value.replace(/\+/g,' ')) ) ;
		
		if ( value == '1' || value == 'on' ) $('input[type="checkbox"][name="'+name+'"]').prop('checked', true);
		
	} ) ;
	
	function wait2load_ns () {
		if ( namespaces_loading ) {
			setTimeout ( wait2load_ns , 100 ) ;
			return ;
		}
		
		loadNamespaces() ;
	}
	wait2load_ns() ;
	
	var q = decodeURIComponent ( $('#querystring').text() ) ;
	if ( q != '' ) setPermalink ( q ) ;

	if ( typeof params.active_tab != 'undefined' ) {
		var tab = '#' + params.active_tab.replace(/ /g,'_') ; //$('input[name="active_tab"]').val() ;
		$('#main_form ul.nav-tabs a[href="'+tab+'"]').click() ;
	}
	
	$('body').show() ;
}

function setInterfaceLanguage ( l ) {
	if ( interface_language == l ) return false ;
	interface_language = l ;
	$('input[name="interface_language"]').val ( l ) ;
	$.each ( interface_text['en'] , function ( k , v ) {
		if ( typeof interface_text[l][k] != 'undefined' ) v = interface_text[l][k] ;
		$('.l_'+k).html ( v ) ;
		$('.ph_'+k).attr ( { placeholder:v } ) ;
	} ) ;
	$('a.l_manual').attr ( { href:'https://meta.wikimedia.org/wiki/PetScan/'+l } ) ;
	$('a.l_interface_text').attr ( { href:'https://meta.wikimedia.org/wiki/PetScan/Interface#'+l.toUpperCase() } ) ;
//	if ( typeof autolist != 'undefined' ) autolist.setInterfaceLanguage ( l ) ;
	$('#doit').attr ( { value : _t('doit') } ) ;
	return false ;
}

function loadInterface ( init_lang , callback ) {
	$.getJSON ( 'https://meta.wikimedia.org/w/api.php?action=parse&prop=wikitext&page=PetScan/Interface&format=json&callback=?' , function ( d ) {
		var wt = d.parse.wikitext['*'] ;
		var rows = wt.split("\n") ;
		var lang = '' ;
		$.each ( rows , function ( k , v ) {
			var m = v.match ( /^==\s*(.+?)\s*==$/ ) ;
			if ( m != null ) {
				lang = m[1].toLowerCase() ;
				interface_text[lang] = {} ;
				
				var h = '<a class="dropdown-item" href="#">' + m[1] + '</a>' ;
				$('#interface_languages').append ( h ) ;
				return ;
			}
			m = v.match ( /^;(.+?):(.*)$/ ) ;
			if ( m == null ) return ;
			if ( m[1] == 'toolname' ) m[2] = 'PetScan' ; // HARDHACK FIXME
			interface_text[lang][m[1]] = m[2] ;
		} ) ;
		
		$('#interface_languages a.dropdown-item').click ( function (e) {
			e.preventDefault();
			var o = $(this) ;
			var lang = o.text().toLowerCase() ;
			setInterfaceLanguage ( lang ) ;
		} ) ;
		
		setInterfaceLanguage ( init_lang ) ;

		loadNamespaces() ;
		$('input[name="language"]').keyup ( loadNamespaces ) ;
		$('input[name="project"]').keyup ( loadNamespaces ) ;
		
		applyParameters() ;
		if ( typeof callback != 'undefined' ) callback() ;
	} ) ;
}

function loadNamespaces () {
	if ( namespaces_loading ) return false ;
	var l = $('input[name="language"]').val() ;
	if ( l.length < 2 ) return false ;
	var p = $('input[name="project"]').val() ;
	if ( l == 'wikidata' ) { l = 'www' ; p = 'wikidata' ; }
	var lp = l+'.'+p ;
	if ( lp == last_namespace_project ) return false ;
	
	var server = lp+'.org' ;
	namespaces_loading = true ;
	$.getJSON ( 'https://'+server+'/w/api.php?action=query&meta=siteinfo&siprop=namespaces&format=json&callback=?' , function ( d ) {
		namespaces_loading = false ;
		if ( typeof d == 'undefined' ) return ;
		if ( typeof d.query == 'undefined' ) return ;
		if ( typeof d.query.namespaces == 'undefined' ) return ;
		last_namespaces = {} ;
		max_namespace_id = 0 ;
		$.each ( d.query.namespaces , function ( id , v ) {
			id *= 1 ;
			if ( id < 0 ) return ;
			var title = v['*'] ;
			if ( title == '' ) title = _t('namespace_0',l) ;
			last_namespaces[id] = [ title , (v.canonical||title) ] ;
			if ( id > max_namespace_id ) max_namespace_id = id ;
		} ) ;
		
		last_namespace_project = lp ;
		
		var nsl = [] ;
		function renderNS ( ns ) {
			var h = "<div>" ;
			if ( typeof last_namespaces[ns] != 'undefined' ) {
				h += "<label" ;
				if ( last_namespaces[ns][0] != last_namespaces[ns][1] ) h += " title='"+(last_namespaces[ns][1]||'')+"'" ;
				h += "><input type='checkbox' value='1' ns='"+ns+"' name='ns["+ns+"]'" ;
				if ( $.inArray ( ns , namespaces_selected ) >= 0 ) {
					nsl.push ( ns ) ;
					h += " checked" ;
				}
				h += "> " ;
				h += last_namespaces[ns][0] ;
				h += "</label>" ;
			} else h += "&mdash;" ;
			h += "</div>" ;
			return h ;
		}
		
		$(".current_wiki").text ( lp ) ;
		h = "" ;
		h += "<div class='smaller'>" ;
		for ( var ns = 0 ; ns <= max_namespace_id ; ns += 2 ) {
			if ( typeof last_namespaces[ns] == 'undefined' && typeof last_namespaces[ns+1] == 'undefined' ) continue ;
			h += "<div class='ns-block'>" ;
			h += renderNS ( ns ) ;
			h += renderNS ( ns+1 ) ;
			h += "</div>" ;
		}
		h += "</div>" ;
		namespaces_selected = nsl ;
		
		$('#namespaces').html ( h ) ;
		$('#namespaces input').change ( function () {
			var o = $(this) ;
			var ns = o.attr('ns') * 1 ;
			namespaces_selected = jQuery.grep(namespaces_selected, function(value) { return value != ns; }); // Remove is present
			if ( o.is(':checked') ) namespaces_selected.push ( ns ) ;
		} ) ;
		
	} ) . always ( function () {
		namespaces_loading = false ;
	} ) ;
	return false ;
}

var autolist ;


// Converts a WDQ input box to SPARQL via wdq2sparql, if possible
function wdq2sparql ( wdq , sparql_selector ) {
	$(sparql_selector).prop('disabled', true);
	
	$.get ( 'https://tools.wmflabs.org/wdq2sparql/w2s.php' , {
		wdq:wdq
	} , function ( d ) {
		$(sparql_selector).prop('disabled', false);
		if ( d.match ( /^<!DOCTYPE/ ) ) {
			alert ( _t('wdq2sparql_fail') ) ;
			return ;
		}
		d = d.replace ( /^prefix.+$/mig , '' ) ;
		d = d.replace ( /\s+/g , ' ' ) ;
		d = $.trim ( d ) ;
		$(sparql_selector).val ( d ) ;
	} ) ;
	
	return false ;
}

function initializeInterface () {
	var p = getUrlVars() ;
	

	// Ensure NS0 is selected by default
	var cnt = 0 ;
	$.each ( p , function ( k , v ) { cnt++ } ) ;
	if ( cnt == 0 ) p['ns[0]'] = 1 ;
	
	// Legacy parameters
	if ( typeof p.comb_subset != 'undefined' ) p.combination = 'subset' ;
	if ( typeof p.comb_union != 'undefined' ) p.combination = 'union' ;
	if ( typeof p.get_q != 'undefined' ) p.wikidata_item = 'any' ;
	if ( typeof p.wikidata != 'undefined' ) p.wikidata_item = 'any' ;
	if ( typeof p.wikidata_no_item != 'undefined' ) p.wikidata_item = 'without' ;
	if ( typeof p.giu != 'undefined' ) p.file_usage_data = 'on' ;
	
	

	params = $.extend ( {} , default_params , p ) ;
	
	var l = 'en' ;
	if ( typeof params.interface_language != 'undefined' ) l = params.interface_language ;
	
	loadInterface ( l , function () {
		autolist = new AutoList () ;
	} ) ;
	$('input[name="language"]').focus() ;
/*	$('#doit').click ( function () {
		$('#main_form').submit() ;
	} ) ;*/
	
	$('#main_form ul.nav-tabs a').click ( function (e) {
		e.preventDefault() ;
		var o = $(this) ;
		$('input[name="active_tab"]').val ( o.attr('href').replace(/^\#/,'') ) ;
	} ) ;
	
	$('#wdq2sparql').click ( function ( e ) {
		e.preventDefault() ;
		var wdq = prompt ( _t('wdq2sparql_prompt') , '' ) ;
		if ( wdq == null || $.trim(wdq) == '' ) return false ;
		wdq2sparql ( wdq , 'input[name="sparql"]' ) ;
		return false ;
	} ) ;
	
	function highlightMissingWiki () {
		var o = $('textarea[name="manual_list"]') ;
		var wo = $('input[name="manual_list_wiki"]') ;
		var text = o.val() ;
		var wiki = wo.val() ;
		var wop = $(wo.parents("div.input-group").get(0)) ;
		if ( $.trim(text) != '' && !wiki.match(/wiki\s*$/) ) { //$.trim(wiki) == '' ) {
			wop.addClass ( 'has-danger' ) ;
			$('#doit').prop('disabled', true)
		} else {
			wop.removeClass ( 'has-danger' ) ;
			$('#doit').prop('disabled', false)
		}
	}
	$('textarea[name="manual_list"]').keyup ( highlightMissingWiki ) ;
	$('input[name="manual_list_wiki"]').keyup ( highlightMissingWiki ) ;
	highlightMissingWiki() ;
	
	$('#tab-list').click ( function () {
		if ( $('#main_form div.tab-pane').length > 0 ) {
			$('#tab-list').text ( "Tabs" ) ;
			$('#main_form ul.nav-tabs').hide() ;
			$('#main_form div.tab-pane').addClass('former-tab-pane').removeClass('tab-pane') ;
		} else {
			$('#tab-list').text ( "List" ) ;
			$('#main_form ul.nav-tabs').show() ;
			$('#main_form div.former-tab-pane').addClass('tab-pane').removeClass('former-tab-pane') ;
		}
	} ) ;
	
	// Deactivate REDIRECTS when showing only pages without item. This will be a mess otherwise, few users would think to change that setting. It can always be changed back manually.
	$('input[name="wikidata_item"][value="without"]').click ( function ( e ) {
		$('input[name="show_redirects"][value="no"]').prop('checked', true);
	} ) ;
	
	$('#quick_commons').click ( function ()     { $("input[name='language']").val("commons");  $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;
	$('#quick_wikispecies').click ( function () { $("input[name='language']").val("species");  $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;
	$('#quick_wikidata').click ( function ()    { $("input[name='language']").val("wikidata"); $("input[name='project']").val("wikimedia"); loadNamespaces() ; return false } ) ;

}

$(document).ready ( function () {
	initializeInterface() ;
} ) ;