package main

import (
	"fmt"
	"io"
	"net/http"
	"runtime"
	"time"
)

func dbFetch(key string) string {
	time.Sleep(10 * time.Millisecond)
	return "db_result:" + key
}

func serviceCall(raw string) string {
	time.Sleep(10 * time.Millisecond)
	return "enriched:" + raw
}

func formatResponse(enriched string) string {
	return "formatted:" + enriched + "\n"
}

func main() {
	runtime.GOMAXPROCS(runtime.NumCPU())

	mux := http.NewServeMux()

	mux.HandleFunc("/ping", func(w http.ResponseWriter, r *http.Request) {
		fmt.Fprint(w, "pong\n")
	})

	mux.HandleFunc("/echo", func(w http.ResponseWriter, r *http.Request) {
		body, _ := io.ReadAll(r.Body)
		if len(body) == 0 {
			fmt.Fprint(w, "(empty)\n")
		} else {
			w.Write(body)
		}
	})

	// Each request runs in its own goroutine — Go's equivalent of our coroutine pipeline.
	// Blocking sleeps are fine here: goroutines are cheap (~2KB stack vs ~1MB OS thread).
	mux.HandleFunc("/pipeline", func(w http.ResponseWriter, r *http.Request) {
		raw      := dbFetch("pipeline")
		enriched := serviceCall(raw)
		response := formatResponse(enriched)
		fmt.Fprint(w, response)
	})

	fmt.Printf("Go server listening :8081  GOMAXPROCS=%d\n", runtime.GOMAXPROCS(0))
	http.ListenAndServe(":8081", mux)
}
