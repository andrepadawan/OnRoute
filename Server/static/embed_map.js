var map = null;
var shuttleIcon = null;
var poi_list = null;
var markers = new Map();
let shuttle = null

function map_init(){
         map = L.map('map').setView([45.0703, 7.6869], 15);
        L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
            }).addTo(map);
        read_poi()
        icons_init()
        showLivePosition()
        map.invalidateSize()
}

function icons_init(){
    shuttleIcon = L.divIcon({
        className:'shuttle-div-icon',
        iconSize: [20, 20],
        html: '<div class="shuttle-dot"></div>'
    })
}


function read_poi() {
    poi_list = JSON.parse(document.getElementById('poi-data').textContent);
    console.log(poi_list)
    poi_list.forEach(p => {
        const m = L.marker([p.lat, p.lon]).bindPopup(p.name).addTo(map);
        markers.set(p.id, m)
    })

             if (poi_list.length > 0) {
                 const coordPoi = poi_list.map(p => [p.lat, p.lon]);
                 map.fitBounds(coordPoi, {padding: [10, 10]})
                 //Adding a listener
                 //map.on('click', onMapClick)
                 //Since Leafleat does not like having a hidden map in a dialogue
                 map.invalidateSize() //Forces map to recalculate tiles
             }
         }

function showLivePosition() {
    coords_list = JSON.parse(document.getElementById('coords').textContent);
    shuttle = L.marker([45.0703, 7.6869], {icon:shuttleIcon}).addTo(map)
}