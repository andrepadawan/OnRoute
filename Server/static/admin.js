 var poi_map = null;
 var popup = L.popup()
 var poi_list = []
 var markers = new Map()


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
                         init_map()
                         //Adding markers where our POI are, binding each one with its own name
                         read_poi()
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

         function init_map() {
            poi_map = L.map('poi-map').setView([45.0703, 7.6869] , 15);
            L.tileLayer('https://tile.openstreetmap.org/{z}/{x}/{y}.png',
                         {
                             maxZoom: 19,
                             attribution: '&copy; <a href="http://www.openstreetmap.org/copyright">OpenStreetMap</a>'
                         }).addTo(poi_map)
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
