// placeholder of API peers response

var peer_resonse=[];//=[responseBTCD,responseBTC];

var currentCoin=0;
/*
 * Gets peer list using postCall or using AJAX GET request
 * (further code will need modifications when native call will be implemented)
 * (Above initialized usepostCall variable must be set to true if postCall is to be used)
 * 
 */
function getPeerList(){
    var coin_types = coinManagement.getCoinSymbols();
    if(currentCoin<coin_types.length){
     var tag = tagGen(18);
        console.log("Inside getPeerList");
        var request=
                '{"agent":"iguana","method":"peers","coin":'+'"'+coin_types[currentCoin]+'","tag":"' + tag.toString() + '"}';
currentCoin++;
//console.log('Requesting: ' + request);
SPNAPI.makeRequest(request,addpeer_toresponse);
//SPNAPI.makeRequest(request,getPeerList());
    }else{
    currentCoin=0;
    renderPeersGrid();
    }
}


function addpeer_toresponse(request, response){
    
    var data=JSON.parse(response);
       if(data.error && data.error==="peers needs coin"){
           console.log("Coin not present");
           
       }else{
           peer_resonse.push(data);
       }
       getPeerList();
}
/**
 * 
 * @param {sting} ip (ip address current of peer)
 * @param {string} coin (coin short name)
 * @returns {undefined}
 * 
 */

function connectPeer(ip,coin){
    
    var request='{"agent":"iguana","method":"addnode","ipaddr":'+'"'+ip+'","coin":"' + coin + '"}';
    console.log("connection to peer:"+ip+" coin:"+coin);
    SPNAPI.makeRequest(request,function(request,response){
            //console.log('Response is ' + response);
            var res=JSON.parse(response);
            if(res.result==="addnode submitted"){
                addPeerToConn(ip,coin);
        }
        });
}

/**
 * 
 * @param {sting} ip (ip address current of peer)
 * @param {string} coin (coin short name)
 * @returns {undefined}
 * 
 */
function disconnectPeer(ip,coin){
    
    var request='{"agent":"iguana","method":"removenode","ipaddr":'+'"'+ip+'","coin":"' + coin + '"}';
    console.log("disconnection to peer:"+ip+" coin:"+coin);
    SPNAPI.makeRequest(request, function(request,response){
            //console.log('Response is ' + response);
            removePeerFromConn(ip,coin);
        });


}


var favPeers = [];
/**
 * 
 * @type Array
 * (used to store connected peer in a string format)
 */

var connectedPeers=[];
var getHtmlRow = function (id,coin, peer) {
    var row = '';
    var data=id+coin;
    row = '<tr data-id="' + data.toString() + '">';
    row += '<td>' + peer.ipaddr + '</td>';
    row += '<td>' + peer.cointype + '</td>';
    row += '<td>' + peer.height + '</td>';
    row += '<td>' + peer.rank + '</td>';
    if ($.inArray(data, favPeers) === -1) {
        row += '<td><button class="btn btn-xs btn-success btn-raised addPeerToFav" data-coin="'+peer.cointype.toString()+'" data-id="' + id.toString() + '"> + Favorite</button></td>';
        // row += '<td><i class="material-icons addPeerToFav" data-id="' + id.toString() + '">bookmark_border</i></td>';
    }
    else {
        row += '<td><button class="btn btn-xs btn-danger btn-raised removePeerFromFav" data-coin="'+peer.cointype.toString()+'" data-id="' + id.toString() + '"> - Unfavorite</button></td>';
        // row += '<td><i class="material-icons removePeerFromFav" data-id="' + id.toString() + '">bookmark</i></td>'
     }
    
    if ($.inArray(peer.ipaddr.toString()+" "+peer.cointype.toString(), connectedPeers) === -1) {
        row += '<td><button class="btn btn-xs btn-success btn-raised connectPeer" data-ip="'+peer.ipaddr.toString()+'" data-coin="'+peer.cointype.toString()+'" data-id="' + id.toString() + '"> + Connect</button>';
        row +='</td>'; 
        
    }else{
        row += '<td><button class="btn btn-xs btn-danger btn-raised disconnectPeer" data-ip="'+peer.ipaddr.toString()+'" data-coin="'+peer.cointype.toString()+'" data-id="' + id.toString() + '"> -Disconnect</button>';
        row +='</td>'; 
    }
    row += '</tr>';
    return row;
};


var addPeerToConn = function (ip,coin) {
    if ($.inArray(ip+" "+coin, connectedPeers) === -1) {
        connectedPeers.push(ip+" "+coin);
        console.log('@ peer connected', connectedPeers);
    }

    // refresh grid e.getAttribute('data-id'),e.getAttribute('data-coin')
    renderPeersGrid(document.getElementById('cbShowFavoritePeers').checked);
};

var removePeerFromConn = function (ip,coin) {
    for (var index = 0; index < connectedPeers.length; index++) {
        if (ip+" "+coin === connectedPeers[index]) {
            connectedPeers.splice(index, 1);
            console.log('@ peer disconnected', connectedPeers);
            break;
        }
    }

    // refresh grid
    renderPeersGrid(document.getElementById('cbShowFavoritePeers').checked);
};



var addPeerToFav = function (id,coin) {
    if ($.inArray(id+coin, favPeers) === -1) {
        favPeers.push(id+coin);
        console.log('@ peer faved', favPeers);
    }

    // refresh grid e.getAttribute('data-id'),e.getAttribute('data-coin')
    renderPeersGrid(false);
};

var removePeerFromFav = function (id,coin) {
    for (var index = 0; index < favPeers.length; index++) {
        if (id+coin === favPeers[index]) {
            favPeers.splice(index, 1);
            console.log('@ peer unfaved', favPeers);
            break;
        }
    }

    // refresh grid
    renderPeersGrid(document.getElementById('cbShowFavoritePeers').checked);
};
/*
var renderPeersGrid = function (favoritesOnly = false) {

    console.log('@ peer print grid');
 
    var peersTableAllHtml = '';

    for (var i = 0; i < response.peers.length; i++) {

        if (favoritesOnly == true && $.inArray(i, favPeers) == -1) {
            continue;
        }

        response.peers[i].cointype = response.coin
        peersTableAllHtml += getHtmlRow(i, response.peers[i]);
    }
    document.getElementById('peersTableBody').innerHTML = peersTableAllHtml;
};*/
var favoritesOnly=false;

var renderPeersGrid = function () {

    console.log('@ peer print grid');
 
    var peersTableAllHtml = '';
    for(var j=0; j<peer_resonse.length;j++){
      var res=peer_resonse[j];
        for (var i = 0; i < res.peers.length; i++) {
var data=i+res.coin;
        if (favoritesOnly === true && $.inArray(data.toString(), favPeers) === -1) {
            continue;
        }
        //console.log(data.toString());
        res.peers[i].cointype = res.coin;
        peersTableAllHtml += getHtmlRow(i,res.coin, res.peers[i]);
    }
        
    }
    
    document.getElementById('peersTableBody').innerHTML = peersTableAllHtml;
};


document.getElementById('cbShowFavoritePeers').onclick = function () {
    // if (document.getElementById('cbShowFavoritePeers').checked == true) {

    //     // document.getElementById('peersTableBody').style.display = 'none';
    //     // document.getElementById('peersTable_fav').style.display = 'block';

    //     $('#peersTableBody').hide();
    //     $('#peersTable_fav').show();
    // }
    // else {

    //     // document.getElementById('peersTableBody').style.display = 'block';
    //     // document.getElementById('peersTable_fav').style.display = 'none';

    //     $('#peersTableBody').show();
    //     $('#peersTable_fav').hide();
    // }
favoritesOnly=document.getElementById('cbShowFavoritePeers').checked;
    renderPeersGrid();
};

var startPeerManagement = function () {
    renderPeersGrid();

};
