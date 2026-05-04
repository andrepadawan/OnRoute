var poi_map = null;
var preview_map = null;
var popup = L.popup()
var poi_list = []
var markers = new Map()

poi_list = JSON.parse(document.getElementById('poi-data').textContent);

function openPOIDialogue(mode, data){
    const dialogue = document.getElementById('dialogue-poi')
    if(mode == 'add'){
        setAddMode()
    } else {
        //getting the fields
        setModifyMode(data)
    }
    dialogue.showModal()
    setTimeout(function () {
        if (poi_map==null) {
            poi_map = init_map('poi-map')
            //Adding markers where our POI are, binding each one with its own name
            add_listener(poi_list, poi_map)
            read_poi(poi_list, poi_map)
            poi_map.invalidateSize();
        }
        else {
            //Dialogue already opened, map is alright. We only need to recalculate the size
            poi_map.invalidateSize();
        }
        if(mode === 'modify'){
            markers.get(data.id).openPopup()
        }
        }, 200)
}

        function closePOIDialogue(){
            const dialogue = document.getElementById('dialogue-poi')
            const form = dialogue.querySelector('form')
            form.reset()
            poi_map?.closePopup()
            dialogue.close()
        }

        function setAddMode(){
            const dialogue = document.getElementById('dialogue-poi')
            const form = dialogue.querySelector('form')
            const button = document.querySelector('[data-action="add-poi"]')
            form.reset()
            form.action = '/admin/add-poi'
            button.textContent = 'Add'
            return form
        }

        function setModifyMode(data){
            const dialogue = document.getElementById('dialogue-poi')
            const form = dialogue.querySelector('form')
            const button = form.querySelector('[data-action="modify-poi"]')
            form.action = '/admin/modify-poi'
            button.textContent = 'Modify'
            form.querySelector('[name="name"]').value = data.name
            form.querySelector('[name="lat"]').value = data.lat
            form.querySelector('[name="lon"]').value = data.lon
            form.querySelector('[name="id"]').value = data.id
            return form
        }

         function init_map(container_id) {
            const map = L.map(container_id).setView([45.0703, 7.6869] , 15);
            L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                         {
                             maxZoom: 19,
                             attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
                         }).addTo(map)
             return map
         }

         function add_listener(poi_list, map){
            poi_list.forEach(p => {
                 const m = L.marker([p.lat, p.lon]).bindPopup(p.name).addTo(map);
                 m.on('click', () => setModifyMode(p));
                 markers.set(p.id, m)
             })
             //Adding a listener
             poi_map.on('click', onMapClick)
         }

         function read_poi(poi_list, map) {
             if (poi_list.length > 0) {
                 const coordPoi = poi_list.map(p => [p.lat, p.lon]);
                 map.fitBounds(coordPoi, {padding: [25, 25]})
                 //Since Leafleat does not like having a hidden map in a dialogue
                 map.invalidateSize() //Forces map to recalculate tiles
             }
         }

         function read_poi_prev(poi_list, map){
            poi_list.forEach(p => {
                 const m = L.marker([p.lat, p.lon]).bindPopup(p.name).addTo(map);
             })
             read_poi(poi_list, map)

         }

         function onMapClick(e) {
             poi_map.closePopup()
             popup
                 .setLatLng(e.latlng)
                 .setContent(`Lat: ${formatCoordinate(e.latlng.lat)}, Lon: ${formatCoordinate(e.latlng.lng)}`)
                 .openOn(poi_map)
             poi_map.setView([e.latlng.lat, e.latlng.lng])
             const form = setAddMode()
             form.querySelector('[name="lat"]').value = formatCoordinate(e.latlng.lat)
             form.querySelector('[name="lon"]').value = formatCoordinate(e.latlng.lng)
         }

         function formatCoordinate(numb){
         return numb.toFixed(4)
 }


         document.querySelectorAll('[data-action="modify-shift"]').forEach(btn => { btn.addEventListener('click', () => {
             const dialog = document.getElementById('modify-shift-dialogue');
             dialog.querySelector('[name="start_date"]').value = btn.dataset.startDate;
             dialog.querySelector('[name="start_time"]').value = btn.dataset.startTime;
             dialog.querySelector('[name="end_date"]').value = btn.dataset.endDate;
             dialog.querySelector('[name="end_time"]').value = btn.dataset.endTime;
             dialog.querySelector('[name="id"]').value = btn.dataset.id
             dialog.showModal();
            });
         })

        document.querySelectorAll('[data-action="modify-poi"]').forEach(btn => { btn.addEventListener('click', () => {
             const dialogue = document.getElementById('dialogue-poi');
             openPOIDialogue('modify', btn.dataset)})
        });


    function is_active_pill(){
    const status = document.getElementById('status_is_active');
    const pill = document.querySelector('.pill')
        const text_pill=document.getElementById('text-pill')
        if(status.textContent==='true'){
            pill.classList.remove('pill-failure')
            pill.classList.add('pill-success')
            text_pill.textContent='Servizio attivo'
        } else {
            pill.classList.remove('pill-success')
            text_pill.textContent='Servizio non attivo'
            pill.classList.add('pill-failure')
        }
    }

preview_map = init_map('preview-map')
read_poi_prev(poi_list, preview_map)
preview_map.removeControl(preview_map.zoomControl);
preview_map.invalidateSize()
