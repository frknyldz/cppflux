use axum::{
    body::Bytes,
    routing::{get, post},
    Router,
};
use std::net::SocketAddr;
use tokio::time::{sleep, Duration};

async fn db_fetch(key: &str) -> String {
    sleep(Duration::from_millis(10)).await;
    format!("db_result:{key}")
}

async fn service_call(raw: &str) -> String {
    sleep(Duration::from_millis(10)).await;
    format!("enriched:{raw}")
}

async fn ping() -> &'static str {
    "pong\n"
}

async fn pipeline() -> String {
    let raw = db_fetch("pipeline").await;
    let enriched = service_call(&raw).await;
    format!("formatted:{enriched}\n")
}

async fn echo(body: Bytes) -> String {
    if body.is_empty() {
        "(empty)\n".to_string()
    } else {
        String::from_utf8_lossy(&body).into_owned()
    }
}

#[tokio::main]
async fn main() {
    let app = Router::new()
        .route("/ping", get(ping))
        .route("/pipeline", get(pipeline))
        .route("/echo", post(echo));

    let addr = SocketAddr::from(([0, 0, 0, 0], 8083));
    let listener = tokio::net::TcpListener::bind(addr).await.unwrap();
    eprintln!("Rust/Axum server listening :8083");
    axum::serve(listener, app).await.unwrap();
}
