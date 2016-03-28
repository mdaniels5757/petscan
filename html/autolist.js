function WiDaR ( callback ) {
	
	this.is_logged_in = false ;
	this.server = 'https://tools.wmflabs.org' ;
	this.api = this.server+'/widar/index.php?callback2=?' ;
	this.userinfo = {} ;
	this.tool_hashtag = '' ;
	
	this.isLoggedIn = function () {
		return this.is_logged_in ;
	}
	
	this.getInfo = function () {
		var me = this ;
		this.abstractCall ( { 
			action:'get_rights'
		} , function ( d ) {
			me.is_logged_in = false ;
			me.userinfo = {} ;
			if ( typeof (((d||{}).result||{}).query||{}).userinfo == 'undefined' ) {
				callback() ;
				return ;
			}
			me.userinfo = d.result.query.userinfo ;
			if ( typeof me.userinfo.name != 'undefined' ) me.is_logged_in = true ;
			callback() ;
		} ) ;
	}
	
	this.getLoginLink = function ( text ) {
		var h = "<a target='_blank' href='"+this.server+"/widar/index.php?action=authorize'>" + text + "</a>" ;
		return h ;
	}
	
	this.isBot = function () {
		if ( !this.isLoggedIn() ) return false ;
		if ( typeof this.userinfo.groups == 'undefined' ) return false ;
		return -1 != $.inArray ( 'bot' , this.userinfo.groups ) ;
	}
	
	this.getUserName = function () {
		if ( !this.isLoggedIn() ) return ;
		return this.userinfo.name ;
	}
	
	this.abstractCall = function ( params , callback ) {
		var me = this ;
		params.tool_hashtag = me.tool_hashtag ;
		params.botmode = 1 ;
		$.getJSON ( me.api , params , function ( d ) {
			if ( typeof callback != 'undefined' ) callback ( d ) ;
		} ) . fail ( function () {
			if ( typeof callback != 'undefined' ) callback () ;
		} ) ;
	}
	
	this.genericAction = function ( o , callback ) {
		this.abstractCall ( { 
			action:'generic',
			json:JSON.stringify(o)
		} , callback ) ;
	}
	
	this.removeClaim = function ( id , callback ) {
		this.abstractCall ( {
			action:'remove_claim',
			id:id
		} , callback ) ;
	}
	
	this.setClaim = function ( q , prop , target_q , callback ) {
		this.abstractCall ( { 
			action:'set_claims',
			ids:q,
			prop:prop,
			target:target_q
		} , callback ) ;
	}
	
	
	this.getInfo() ;
}

//________________________________________________________________________________________________________________________________________________

function AutoList ( callback ) {

	this.running = [] ;
	this.max_concurrent = 1 ; // 1 thread for non-bot user
	this.delay = 2000 ; // 2 sec delay for non-bot user

	this.setupCommands = function () {
		var me = this ;
		me.commands_todo = [] ;
		me.running = [] ;
		var rows = $('#al_commands').val().split("\n") ;
		$('#main_table input.qcb').each ( function () {
			var o = $(this) ;
			if ( !o.is(':checked') ) return ;
			var q = o.attr('q') ;
			var remove_q ;
			$.each ( rows , function ( k , row ) {
				var cmd = { q:q , status:'waiting' } ;
				var m = row.match ( /^\s*-(P\d+)\s*$/ ) ;
				if ( m == null ) {
					m = row.match ( /^\s*(P\d+)\s*:\s*(Q\d+)\s*$/i ) ;
					if ( m != null ) {
						cmd.mode = 'add' ;
						cmd.prop = m[1] ;
						cmd.value = m[2] ;
					} else return ;
				} else { // Delete property
					cmd.mode = 'delete' ;
					cmd.prop = m[1] ;
				}
				remove_q = me.commands_todo.length ;
				me.commands_todo.push ( cmd ) ;
			} ) ;
			if ( typeof remove_q != 'undefined' ) me.commands_todo[remove_q].remove_q = true ;
		} ) ;
	}
	
	this.finishCommand = function ( id ) {
		var me = this ;
		me.commands_todo[id].status = 'done' ;
		var cmd = me.commands_todo[id] ;
		
		if ( cmd.remove_q ) { // Last for this q, remove row
			$($('#main_table input.qcb[q="'+cmd.q+'"]').parents('tr').get(0)).remove() ;
		}
		
		var tmp = [] ;
		$.each ( me.running , function ( k , v ) {
			if ( v != id ) tmp.push ( v ) ;
		} ) ;
		me.running = tmp ;
		
		setTimeout ( function () { me.runNextCommand() } , me.delay ) ;
	}
	
	this.runCommand = function ( id ) {
		var me = this ;
		me.running.push ( id ) ;
		me.commands_todo[id].status = 'running' ;
		var cmd = me.commands_todo[id] ;
		
		if ( cmd.mode == 'add' ) {
			me.widar.setClaim ( 'Q'+cmd.q , cmd.prop , cmd.value , function ( d ) {
				// TODO error log
				me.finishCommand ( id ) ;
			} ) ;
		} else if ( cmd.mode == 'delete' ) {
			$.getJSON ( 'https://www.wikidata.org/w/api.php?action=wbgetentities&ids=Q'+cmd.q+'&format=json&callback=?' , function ( d ) {
				var done = false ;
				$.each ( ((((d.entities||{})['Q'+cmd.q]||{}).claims||{})[cmd.prop.toUpperCase()]||{}) , function ( k , v ) {
					done = true ;
					me.widar.removeClaim ( v.id , function ( d ) {
						// TODO error log
						console.log ( d ) ;
						me.finishCommand ( id ) ;
					} ) ;
					return false ;
				} ) ;
				if ( !done ) {
					// TODO error log
					me.finishCommand ( id ) ;
				}
			} ) . fail ( function () {
				// TODO error log
				me.finishCommand ( id ) ;
			} ) ;
		} else {
			
		}
	}
	
	this.allDone = function () {
		$('#al_do_stop').hide() ;
		$('#al_do_process').show() ;
		$('#al_status').html ( "<span style='color:green'>"+_t('al_done')+"</span>" ) ;
	}

	this.runNextCommand = function () {
		var me = this ;

		if ( me.running.length >= me.max_concurrent ) {
			setTimeout ( function () { me.runNextCommand } , 100 ) ; // Was already called, so short delay
			return ;
		}
		
		var q_running = {} ;
		$.each ( me.running , function ( k , v ) {
			q_running[me.commands_todo[v]] = 1 ;
		} ) ;

		var run_next ;
		var has_commands_left = false ;
		$.each ( me.commands_todo , function ( k , v ) {
			if ( v.status != 'done' ) has_commands_left = true ;
			if ( v.status != 'waiting' ) return ; // Already running
			if ( typeof q_running[v.q] != 'undefined' ) return ; // Don't run the same q multiple times
			run_next = k ;
			return false ;
		} ) ;

		if ( !has_commands_left ) {
			me.allDone() ;
			return ;
		}
		
		if ( typeof run_next == 'undefined' ) {
			setTimeout ( function () { me.runNextCommand } , 100 ) ; // Was already called, so short delay
			return ;
		}

		me.runCommand ( run_next ) ;
	}

	this.initializeAutoListBox = function () {
		var me = this ;
		var h = '' ;
		var p = getUrlVars() ;
		if ( me.widar.isLoggedIn() ) {
			h += "<div>Welcome, " + me.widar.getUserName() + "!</div>" ;
			if ( me.widar.isBot() ) {
				h += "<div class='l_al_bot'></div>" ;
				me.max_concurrent = 5 ;
				me.delay = 1 ;
			}
			h += "<div class='autolist_subbox'>" ;
			h += "<button id='al_do_check_all' class='btn btn-secondary btn-sm l_al_all'></button><br/>" ;
			h += "<button id='al_do_check_none' class='btn btn-secondary btn-sm l_al_none'></button><br/>" ;
			h += "<button id='al_do_check_toggle' class='btn btn-secondary btn-sm l_al_toggle'></button><br/>" ;
			h += "</div>" ;
			h += "<div class='autolist_subbox'>" ;
			h += "<textarea id='al_commands' class='ph_al_commands_ph' rows=3 style='width:200px'>" + (p.statementlist||'') + "</textarea><br/>" ;
			h += "<button id='al_do_process' class='btn btn-success btn-sm l_al_process'></button>" ;
			h += "<button id='al_do_stop' class='btn btn-danger btn-sm l_al_stop' style='display:none'></button>" ;
			h += "<div id='al_status'></div>" ;
			h += "</div>" ;
		} else {
			h += "<div>" + me.widar.getLoginLink("<span class='l_al_login'></span>") + "</div>" ;
		}
		$('#autolist_box').html ( h ) ;
		
		me.setInterfaceLanguage ( interface_language ) ;
	
	
		$('#al_do_process').click ( function (e) {
			e.preventDefault() ;
			$('#al_do_process').hide() ;
			$('#al_do_stop').show() ;
			$('#al_status').html ( _t('al_running') ) ;
			me.setupCommands() ;

			for ( var i = 0 ; i < me.max_concurrent ; i++ ) {
				me.runNextCommand() ;
			}
		} ) ;

		$('#al_do_stop').click ( function (e) {
			e.preventDefault() ;
		} ) ;
	}

	this.initalizeTable = function () {
		$('#main_table thead tr').prepend ( "<th></th>" ) ;
		$('#main_table tbody tr').each ( function () {
			var o = $(this) ;
			var href = $(o.find("td a").get(0)).attr ( 'href' ) ;
			var m = href.match ( /\/Q(\d+)$/ ) ;
			if ( m == null ) return ;
			var q = m[1] ;
			var h = "<td><input type='checkbox' class='qcb' q='"+q+"' checked></td>" ;
			o.prepend ( h ) ;
		} ) ;
	}
	
	this.setInterfaceLanguage = function ( l ) {
		$.each ( interface_text['en'] , function ( k , v ) {
			if ( !k.match(/^al_/) ) return ; // AutoList only
			if ( typeof interface_text[l][k] != 'undefined' ) v = interface_text[l][k] ;
			$('#autolist_box .l_'+k).html ( v ) ;
			$('#autolist_box .ph_'+k).attr ( { placeholder:v } ) ;
		} ) ;
	}

	var me = this ;
	if ( $('#autolist_box').length == 0 ) { // Don't bother
		if ( typeof callback != 'undefined' ) callback() ;
		return ;
	}

	me.commands_todo = [] ;
	me.initalizeTable() ;
	me.widar = new WiDaR ( function () {
		me.widar.tool_hashtag = 'petscan' ;
		me.initializeAutoListBox() ;
		if ( typeof callback != 'undefined' ) callback() ;
	} ) ;
}
