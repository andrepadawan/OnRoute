var map = null;

function map_init(){
         map = L.map('map').setView([45.0703, 7.6869], 15);
        L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png', {
        maxZoom: 19,
        attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
            }).addTo(map);
        map.invalidateSize()
}

function read_poi() {
             poi_list = JSON.parse(document.getElementById('poi-data').textContent);
             console.log(poi_list)
             poi_list.forEach(p => {
                 const m = L.marker([p.lat, p.lon]).bindPopup(p.name).addTo(poi_map);
                 m.on('click', () => setModifyMode(p));
                 markers.set(p.id, m)
             })

             if (poi_list.length > 0) {
                 const coordPoi = poi_list.map(p => [p.lat, p.lon]);
                 poi_map.fitBounds(coordPoi, {padding: [10, 10]})
                 //Adding a listener
                 poi_map.on('click', onMapClick)
                 //Since Leafleat does not like having a hidden map in a dialogue
                 poi_map.invalidateSize() //Forces map to recalculate tiles
             }
         }