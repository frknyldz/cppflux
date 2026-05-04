#include "cppflux/router.hpp"

#include <gtest/gtest.h>

// ── HttpResult ────────────────────────────────────────────────────────────────

TEST(HttpResult, OkDefaults) {
    auto r = HttpResult::ok("hello");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body, "hello");
    EXPECT_EQ(r.content_type, "text/plain");
}

TEST(HttpResult, OkWithContentType) {
    auto r = HttpResult::ok(R"({"ok":true})", "application/json");
    EXPECT_EQ(r.code, 200);
    EXPECT_EQ(r.body, R"({"ok":true})");
    EXPECT_EQ(r.content_type, "application/json");
}

TEST(HttpResult, Err) {
    auto r = HttpResult::err(404, "Not Found");
    EXPECT_EQ(r.code, 404);
    EXPECT_EQ(r.body, "Not Found");
}

TEST(HttpResult, DefaultConstruct) {
    HttpResult r;
    EXPECT_EQ(r.code, 200);
    EXPECT_TRUE(r.body.empty());
    EXPECT_EQ(r.content_type, "text/plain");
}

// ── Router route registration ─────────────────────────────────────────────────
// handle() needs a real evhttp_request* so we can't call it in a unit test.
// These tests verify that registration compiles and doesn't crash, and that
// the handler type deduction (sync vs async) works correctly.

TEST(Router, RegisterSyncGetHandler) {
    Router router;
    bool   registered = false;
    router.get("/ping", [&](Request, Response) { registered = true; });
    SUCCEED();  // reaching here means registration compiled and ran cleanly
}

TEST(Router, RegisterAsyncGetHandler) {
    Router router;
    router.get("/async", [](Request) -> HttpTask { co_return HttpResult::ok("ok"); });
    SUCCEED();
}

TEST(Router, RegisterSyncPostHandler) {
    Router router;
    router.post("/echo", [](Request, Response) {});
    SUCCEED();
}

TEST(Router, RegisterAsyncPostHandler) {
    Router router;
    router.post("/data", [](Request req) -> HttpTask {
        co_return HttpResult::ok(req.body(), "application/json");
    });
    SUCCEED();
}

TEST(Router, RegisterMultipleRoutes) {
    Router router;
    router.get("/a", [](Request, Response) {});
    router.get("/b", [](Request, Response) {});
    router.post("/c", [](Request, Response) {});
    router.get("/d", [](Request) -> HttpTask { co_return HttpResult::ok("d"); });
    SUCCEED();
}
