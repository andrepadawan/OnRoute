var map = null;
var shuttleIcon = null;
var poi_list = null;
var markers = new Map();
let shuttle = null
var coords_dict = null

function map_init(){
         map = L.map('map').setView([45.0703, 7.6869], 15);
         if (L.Browser.mobile) {
             //Removing + - buttons on mobile
            map.removeControl(map.zoomControl);
        }
        L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
            }).addTo(map);
        map.scrollWheelZoom.disable();
        map.invalidateSize()
        read_poi()
        icons_init()
        shuttle_marker_init()
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

async function fetch_coordinates_loop() {
  const url = "/coordinates";
  try {
    const response = await fetch(url);
    if (!response.ok) {
      throw new Error(`Response status: ${response.status}`);
    }

    coords_dict = await response.json();
    update_position(coords_dict)
  } catch (error) {
    console.error(error.message);
  }
}


window.addEventListener('load', function () {
  //document is loaded.
  var fetchInterval = 10000; // 10 seconds.

  // Invoke the request every 10 seconds.
  setInterval(fetch_coordinates_loop, fetchInterval);
});

function shuttle_marker_init(){
    coords_dict = JSON.parse(document.getElementById('coords').textContent);
    shuttle = L.marker([coords_dict['lat'], coords_dict['lon']], {icon:shuttleIcon}).addTo(map)
}

function update_position(coords_dict){
    shuttle.setLatLng([coords_dict['lat'], coords_dict['lon']])
}