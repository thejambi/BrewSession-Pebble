// Offline shell: cache-first for the app's own files, because a tea timer
// that needs the network is a broken kettle of a different kind.
const CACHE = 'brewsession-web-v2';
const ASSETS = [
  './', './index.html', './style.css', './manifest.webmanifest',
  './js/main.js', './js/windows.js', './js/session.js',
  './js/digits.js', './js/cup.js', './js/colors.js',
  './icons/icon-192.png', './icons/icon-512.png',
];

self.addEventListener('install', (e) => {
  e.waitUntil(caches.open(CACHE).then((c) => c.addAll(ASSETS)));
  self.skipWaiting();
});

self.addEventListener('activate', (e) => {
  e.waitUntil(caches.keys().then((keys) =>
    Promise.all(keys.filter((k) => k !== CACHE).map((k) => caches.delete(k)))));
  self.clients.claim();
});

// Network first, cache as the kettle-with-no-wifi fallback — so a deployed
// update is picked up on the next visit, not two visits later.
self.addEventListener('fetch', (e) => {
  e.respondWith(
    fetch(e.request).then((res) => {
      const copy = res.clone();
      caches.open(CACHE).then((c) => c.put(e.request, copy)).catch(() => {});
      return res;
    }).catch(() => caches.match(e.request))
  );
});
