var map = null;
var shuttleIcon = null;
var poi_list = null;
var markers = new Map();
let shuttle = null
var coords_dict = null
var map_wrapper = document.querySelector(".map-wrapper")

function map_init(){
    //Istanziando la mappa con misure a priori
         map = L.map('map').setView([45.0703, 7.6869], 15);
         //E rimuovo i pulsanti +- perché su mobile tolgono parecchio spazio
         if (L.Browser.mobile) {
            map.removeControl(map.zoomControl);
        }

        // carico il tile layer corretto
        L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
            }).addTo(map);

         //Disattivo per comodità d'uso: rimane cmq possibile spostare mappa e pinch-to-zoom
        map.scrollWheelZoom.disable();
        map.invalidateSize() //Ricarica mappa leaflet
        //Map operations finished
        coords_dict = JSON.parse(document.getElementById('coords').textContent);
        read_poi()//a questo punto i poi sono pronti 

        icons_init()

        map.on('movestart zoomstart', function() {
                if(shuttle && map.hasLayer(shuttle)) {
                    const el = shuttle.getElement()
                    el.classList.remove('is-animated')
                }
        })

        map.on('moveend zoomend', function() {
                if(shuttle && map.hasLayer(shuttle)) {
                    const el = shuttle.getElement()
                    el.classList.add('is-animated')
            }
        })
        render(coords_dict)
}

function icons_init(){
    shuttleIcon = L.divIcon({
        className:'shuttle-div-icon',
        iconSize: [20, 20],
        html: '<div class="shuttle-dot"></div>'
    })
}


function read_poi() {
    const escapeHtml = (str) => String(str)
  .replace(/&/g, '&amp;')
  .replace(/</g, '&lt;')
  .replace(/>/g, '&gt;')
  .replace(/"/g, '&quot;')
  .replace(/'/g, '&#39;');
  
    poi_list = JSON.parse(document.getElementById('poi-data').textContent);
    console.log(poi_list)
    poi_list.forEach(p => {
        const m = L.marker([p.lat, p.lon]).bindPopup(escapeHtml(p.name)).addTo(map);
        markers.set(p.id, m)
    })

             if (poi_list.length > 0) {

                 const coordPoi = poi_list.map(p => [p.lat, p.lon]);
                 map.fitBounds(coordPoi, {padding: [10, 10]})
                 //se c'è shuttle tiene conto anche delle sue coordinate
                //rivedere
                 if(shuttle!=null&&state=="live"){
                    var mapFit = poi_list.map(p => [p.lat, p.lon]);
                    mapFit.push([shuttle.latitude, shuttle.longitude])
                    map.fitBounds(mapFit, {padding: [10, 10]})

                 }
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
    payload = await response.json();
    render(payload)
  } catch (error) {
    console.error(error.message);
  }
}

window.addEventListener('load', function () {
  //document is loaded.
  var fetchInterval = 8000; // 8 seconds.

  // Invoke the request every 8 seconds.
  setInterval(fetch_coordinates_loop, fetchInterval);
});

function shuttle_marker_init(position){
    shuttle = L.marker([position.lat, position.lon], {icon:shuttleIcon}).addTo(map)
    //Adding listener to disable css transformation while zooming map
    shuttle.getElement().classList.add('is-animated')

}

function hide_shuttle_marker(){
    if(shuttle){
        map.removeLayer(shuttle)
        shuttle = null
    }
}

function render(state){
    console.debug(state)
    //using data-* to change shown variable 
    map_wrapper.dataset.status = state?.status
    switch(state?.status){
        case "live":
            //code for live coordinates
            //può esserci caso in cui risulta live ma non ha payload
            if(!(state.payload==null))
                if(shuttle){
                    shuttle.setLatLng([state.payload.lat, state.payload.lon])
                }
                else {
                    shuttle_marker_init(state.payload)
                } 
            else {
                //Errore in cui ho live ma payload non corretto
                map_wrapper.dataset.status = "error"
                hide_shuttle_marker()
                }
            break;
        case "off_service":
            map_wrapper.dataset.status = "off-service"
            break;
        case "no_data":
            map_wrapper.dataset.status = "no-data"
            break;
        case "stale":
            map_wrapper.dataset.status = "stale"
            hide_shuttle_marker()
            break;
        default:
            //hide everything, technical error
            hide_shuttle_marker()
            map_wrapper.dataset.status = "error"
            
    }
}
