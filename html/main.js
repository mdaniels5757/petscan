var params = {} ;
var default_params = {
	'show_redirects':'both',
	'edits[bots]':'both',
	'edits[anons]':'both',
	'edits[flagged]':'both',
	'format':'html'
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
	var hashes = window.location.href.slice(window.location.href.indexOf('?') + 1).split('&');
	if ( hashes.length >0 && hashes[0] == window.location.href ) hashes.shift() ;
	$.each ( hashes , function ( i , j ) {
		var hash = j.split('=');
		hash[1] += '' ;
		vars[decodeURIComponent(hash[0])] = decodeURIComponent(hash[1]).replace(/_/g,' ');
	} ) ;
	return vars;
}

function _t ( k ) {
	var ret = "<i>" + k + "</i>" ;
	if ( typeof interface_text['en'][k] != 'undefined' ) ret = interface_text['en'][k] ;
	if ( typeof interface_text[interface_language][k] != 'undefined' ) ret = interface_text[interface_language][k] ;
	return ret ;
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
		$('input[type="number"][name="'+name+'"]').val ( value*1 ) ;
		$('textarea[name="'+name+'"]').val ( deXSS(value.replace(/\+/g,' ')) ) ;
		
		if ( value == '1' ) $('input[type="checkbox"][name="'+name+'"]').prop('checked', true);
		
	} ) ;
	
	loadNamespaces() ;
	$('body').show() ;
}

function setInterfaceLanguage ( l ) {
	if ( interface_language == l ) return ;
	interface_language = l ;
	$('input[name="interface_language"]').val ( l ) ;
	$.each ( interface_text['en'] , function ( k , v ) {
		if ( typeof interface_text[l][k] != 'undefined' ) v = interface_text[l][k] ;
		$('.l_'+k).html ( v ) ;
	} ) ;
}

function loadInterface ( init_lang ) {
	$.getJSON ( 'https://meta.wikimedia.org/w/api.php?action=parse&prop=wikitext&page=CatScan2/Interface&format=json&callback=?' , function ( d ) {
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
		
		$('#interface_languages a.dropdown-item').click ( function () {
			var o = $(this) ;
			var lang = o.text().toLowerCase() ;
			setInterfaceLanguage ( lang ) ;
		} ) ;
		
		setInterfaceLanguage ( init_lang ) ;

		loadNamespaces() ;
		$('input[name="language"]').keyup ( loadNamespaces ) ;
		$('input[name="project"]').keyup ( loadNamespaces ) ;
		
		applyParameters() ;
	} ) ;
}

function loadNamespaces () {
	if ( namespaces_loading ) return ;
	var l = $('input[name="language"]').val() ;
	if ( l.length < 2 ) return ;
	var p = $('input[name="project"]').val() ;
	var lp = l+'.'+p ;
	if ( lp == last_namespace_project ) return ;
	
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
			if ( title == '' ) title = _t('namespace_0') ;
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
		
//		var h = "<h3><span class='l_namespaces'>" + _t('namespaces') + "</span> " + lp + "</h3>" ;
//		var h = "<div><tt>" + lp + "</tt></div>" ;
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
	;
}

$(document).ready ( function () {
	var p = getUrlVars() ;

	// Ensure NS0 is selected by default
	var cnt = 0 ;
	$.each ( p , function ( k , v ) { cnt++ } ) ;
	if ( cnt == 0 ) p['ns[0]'] = 1 ;

	params = $.extend ( {} , default_params , p ) ;
	
	var l = 'en' ;
	if ( typeof params.interface_language != 'undefined' ) l = params.interface_language ;
	
	loadInterface ( l ) ;
	$('input[name="language"]').focus() ;
	$('#doit').click ( function () {
		$('#main_form').submit() ;
	} ) ;
} ) ;