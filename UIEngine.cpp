//
// Created by PAWEL on 17/10/2026.
//

#include "UIEngine.h"

#include <sstream>
#include <utility>

namespace UIEngine {
    StyleSheet& StyleSheet::rule(std::string selector, StyleDeclarations declarations) { // Add or merge CSS declarations for a given CSS selector
        for (auto& [existingSelector, existingDecls] : _rules) { // Search if the selector already exists in the stylesheet
            if (existingSelector == selector) { // If a matching selector rule exists, merge or overwrite its declarations
                for (auto& [prop, val] : declarations) {
                    existingDecls[prop] = std::move(val);
                }
                return *this;
            }
        }
        _rules.emplace_back(std::move(selector), std::move(declarations)); // Otherwise append a new selector rule
        return *this; // Return reference to *this for fluent method chaining
    }

    void StyleSheet::merge(const StyleSheet& other) { // Merge all rules from another StyleSheet into this one
        for (const auto& [selector, declarations] : other._rules) {
            rule(selector, declarations);
        }
    }

    bool StyleSheet::empty() const { // Check whether the stylesheet contains any rules
        return _rules.empty();
    }

    std::string StyleSheet::toCss() const { // Serialize all rules and declarations into a valid CSS string
        std::ostringstream css;
        for (const auto& [selector, declarations] : _rules) {
            css << selector << " {\n";
            for (const auto& [prop, val] : declarations) {
                css << "    " << prop << ": " << val << ";\n";
            }
            css << "}\n\n";
        }
        return css.str();
    }

    VNode VNode::textNode(std::string content) { // Create a raw text VNode with tag "#text" and the given text content
        return VNode{.tag = "#text", .props = {}, .text = std::move(content), .children = {}}; // Initialize VNode with "#text" tag, empty props, moved text, and no children
    }

    VNode VNode::el(std::string tag, Props props, std::vector<VNode> children) { // Create an HTML element VNode with tag, attributes, and child nodes
        return VNode{.tag = std::move(tag), .props = std::move(props), .text = "", .children = std::move(children)}; // Initialize VNode with moved tag, props, empty text, and moved children
    }

    HTTPServer::Json VNode::toJson() const { // Recursively serialize this VNode and its children into an HTTPServer::Json object
        HTTPServer::Json::Object propsObj; // Create a JSON object map to hold this node's HTML attributes and properties
        for (const auto& [key, val] : props) { // Iterate through each key-value pair in the VNode's Props map
            propsObj[key] = val; // Insert the attribute name and string value into the JSON object
        }

        HTTPServer::Json::Array childrenArr; // Create a JSON array to hold serialized child VNodes
        childrenArr.reserve(children.size()); // Preallocate space for all child VNode JSON objects
        for (const auto& child : children) { // Iterate through each child VNode in order
            childrenArr.push_back(child.toJson()); // Recursively serialize the child VNode and append it to the array
        }

        return HTTPServer::Json::Object{ // Construct and return the complete JSON object representing this VNode
            {"tag", tag},
            {"props", std::move(propsObj)},
            {"text", text},
            {"children", std::move(childrenArr)}
        };
    }

    bool Component::hasProp(const std::string& key) const { // Check whether a specific property key was provided to this component
        return _props.contains(key); // Use std::map::contains to check if the key exists in _props
    }

    std::string Component::getProp(const std::string& key, const std::string& fallback) const { // Retrieve a property value by key, or return fallback if absent
        const auto it = _props.find(key); // Look up the key in the component's _props map
        return it != _props.end() ? it->second : fallback; // Return the found property value, or the fallback string if missing
    }

    namespace {
        std::string buildHtmlShell(const std::string& apiBaseUrl, const std::string& pageTitle) { // Build the minimal HTML shell document containing only the essentials and the Virtual DOM JS runtime
            std::ostringstream html;
            html << R"HTML(<!DOCTYPE html>
<html lang="en">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>)HTML" << pageTitle << R"HTML(</title>
    <link rel="stylesheet" href="/styles.css">
</head>
<body>
    <div id="root"></div>
    <script>
        const API_BASE = ")HTML" << apiBaseUrl << R"HTML(";
        let rawVNodeTemplate = null;
        let currentVNode = null;
        const apiStore = {};

        function resolveUrl(endpoint) {
            if (endpoint.startsWith("http://") || endpoint.startsWith("https://")) return endpoint;
            return API_BASE + (endpoint.startsWith("/") ? endpoint : "/" + endpoint);
        }

        function interpolate(str, item) {
            if (!str || !item) return str;
            return str.replace(/\{(\w+)\}/g, (_, key) => item[key] !== undefined ? String(item[key]) : "");
        }

        function expandVNode(vnode, itemContext = null) {
            if (vnode.tag === "#text") {
                return {
                    tag: "#text",
                    props: {},
                    text: itemContext ? interpolate(vnode.text, itemContext) : vnode.text,
                    children: []
                };
            }

            const props = {};
            for (const [k, v] of Object.entries(vnode.props || {})) {
                props[k] = itemContext ? interpolate(v, itemContext) : v;
            }

            if (props["data-bind-count"]) {
                const list = apiStore[props["data-bind-count"]];
                const count = Array.isArray(list) ? list.length : 0;
                const template = props["data-template"] || "{count}";
                return {
                    tag: vnode.tag,
                    props,
                    text: "",
                    children: [{
                        tag: "#text",
                        props: {},
                        text: template.replace("{count}", String(count)),
                        children: []
                    }]
                };
            }

            if (props["data-for-each"] && (vnode.children || []).length > 0) {
                const endpoint = props["data-for-each"];
                const list = apiStore[endpoint];
                const itemTemplate = vnode.children[0];
                const emptyTemplate = vnode.children[1] || null;
                const expandedChildren = [];

                if (Array.isArray(list) && list.length > 0) {
                    for (const row of list) {
                        expandedChildren.push(expandVNode(itemTemplate, row));
                    }
                } else if (emptyTemplate) {
                    expandedChildren.push(expandVNode(emptyTemplate, null));
                }

                return {
                    tag: vnode.tag,
                    props,
                    text: "",
                    children: expandedChildren
                };
            }

            return {
                tag: vnode.tag,
                props,
                text: vnode.text || "",
                children: (vnode.children || []).map(c => expandVNode(c, itemContext))
            };
        }

        function collectEndpoints(vnode, out = new Set()) {
            const p = vnode.props || {};
            if (p["data-for-each"]) out.add(p["data-for-each"]);
            if (p["data-bind-count"]) out.add(p["data-bind-count"]);
            for (const child of (vnode.children || [])) {
                collectEndpoints(child, out);
            }
            return out;
        }

        async function syncBackendData() {
            if (!rawVNodeTemplate) return;
            const endpoints = collectEndpoints(rawVNodeTemplate);
            await Promise.all(Array.from(endpoints).map(async (ep) => {
                try {
                    const res = await fetch(resolveUrl(ep));
                    apiStore[ep] = await res.json();
                } catch (err) {
                    console.error("Failed to fetch", ep, err);
                }
            }));
            renderTree();
        }

        function renderTree() {
            if (!rawVNodeTemplate) return;
            const nextVNode = expandVNode(rawVNodeTemplate);
            const root = document.getElementById("root");
            if (!currentVNode) {
                root.innerHTML = "";
                root.appendChild(createDomNode(nextVNode));
            } else {
                patchDom(root, currentVNode, nextVNode, 0);
            }
            currentVNode = nextVNode;
        }

        function applyProps(el, oldProps = {}, newProps = {}) {
            for (const key of Object.keys(oldProps)) {
                if (!(key in newProps)) {
                    el.removeAttribute(key);
                }
            }
            for (const [key, value] of Object.entries(newProps)) {
                if (oldProps[key] === value) continue;
                if (key === "data-on-submit") {
                    el.onsubmit = async (e) => {
                        e.preventDefault();
                        const method = newProps["data-method"] || "POST";
                        const payload = {};
                        const formData = new FormData(el);
                        for (const [field, val] of formData.entries()) {
                            payload[field] = val;
                        }
                        try {
                            const res = await fetch(resolveUrl(value), {
                                method,
                                headers: { "Content-Type": "application/json" },
                                body: JSON.stringify(payload)
                            });
                            if (res.ok) {
                                el.reset();
                            }
                            await syncBackendData();
                        } catch (err) {
                            console.error("Form submit failed:", err);
                        }
                    };
                } else if (key === "data-on-click") {
                    el.onclick = async () => {
                        const method = newProps["data-method"] || "GET";
                        try {
                            await fetch(resolveUrl(value), { method });
                            await syncBackendData();
                        } catch (err) {
                            console.error("Action failed:", err);
                        }
                    };
                } else if (!key.startsWith("data-")) {
                    el.setAttribute(key, value);
                }
            }
        }

        function createDomNode(vnode) {
            if (vnode.tag === "#text") {
                return document.createTextNode(vnode.text || "");
            }
            const el = document.createElement(vnode.tag);
            applyProps(el, {}, vnode.props || {});
            for (const child of (vnode.children || [])) {
                el.appendChild(createDomNode(child));
            }
            return el;
        }

        function patchDom(parent, oldVNode, newVNode, index = 0) {
            const existingNode = parent.childNodes[index];
            if (!oldVNode) {
                parent.appendChild(createDomNode(newVNode));
            } else if (!newVNode) {
                if (existingNode) parent.removeChild(existingNode);
            } else if (oldVNode.tag !== newVNode.tag) {
                parent.replaceChild(createDomNode(newVNode), existingNode);
            } else if (newVNode.tag === "#text") {
                if (oldVNode.text !== newVNode.text && existingNode) {
                    existingNode.textContent = newVNode.text;
                }
            } else if (existingNode) {
                applyProps(existingNode, oldVNode.props || {}, newVNode.props || {});
                const oldChildren = oldVNode.children || [];
                const newChildren = newVNode.children || [];
                while (existingNode.childNodes.length > newChildren.length) {
                    existingNode.removeChild(existingNode.lastChild);
                }
                for (let i = 0; i < newChildren.length; i++) {
                    patchDom(existingNode, oldChildren[i], newChildren[i], i);
                }
            }
        }

        async function initApp() {
            const res = await fetch("/vdom");
            rawVNodeTemplate = await res.json();
            renderTree();
            await syncBackendData();
        }

        window.addEventListener("DOMContentLoaded", initApp);
    </script>
</body>
</html>)HTML";
            return html.str();
        }
    }

    void serveApp(
        HTTPServer::Server& frontend,
        std::function<VNode()> rootFactory,
        StyleSheet styleSheet,
        std::string apiBaseUrl,
        std::string pageTitle
    ) { // Register "/", "/styles.css", and "/vdom" routes on the frontend server to serve the C++ VDOM app
        const std::string htmlShell = buildHtmlShell(apiBaseUrl, pageTitle); // Pre-build the minimal HTML shell
        const std::string cssContent = styleSheet.toCss(); // Serialize the C++ StyleSheet into a CSS string

        frontend.get("/", [htmlShell](const HTTPServer::ServerRequest&, HTTPServer::ServerResponse& res) { // Serve the HTML shell at GET /
            HTTPServer::Server::sendHtml(res, HTTP_OK, htmlShell);
        });

        frontend.get("/styles.css", [cssContent](const HTTPServer::ServerRequest&, HTTPServer::ServerResponse& res) { // Serve the generated CSS stylesheet at GET /styles.css
            res.status = HTTP_OK;
            res.contentType = "text/css";
            res.charset = "utf-8";
            res.body = cssContent;
        });

        frontend.get("/vdom", [factory = std::move(rootFactory)](const HTTPServer::ServerRequest&, HTTPServer::ServerResponse& res) { // Serve the live serialized VNode JSON tree at GET /vdom
            HTTPServer::Server::sendJson(res, HTTP_OK, factory().toJson());
        });
    }

    namespace Tags {
        VNode div(Props props, std::vector<VNode> children) { // Create a <div> container element with props and child VNodes
            return VNode::el("div", std::move(props), std::move(children));
        }

        VNode span(Props props, std::vector<VNode> children) { // Create a <span> inline container element with props and child VNodes
            return VNode::el("span", std::move(props), std::move(children));
        }

        VNode span(Props props, const std::string &text) { // Create a <span> inline element containing a single text node
            return VNode::el("span", std::move(props), { VNode::textNode(text) });
        }

        VNode header(Props props, std::vector<VNode> children) { // Create a <header> semantic element with props and child VNodes
            return VNode::el("header", std::move(props), std::move(children));
        }

        VNode main(Props props, std::vector<VNode> children) { // Create a <main> semantic element with props and child VNodes
            return VNode::el("main", std::move(props), std::move(children));
        }

        VNode section(Props props, std::vector<VNode> children) { // Create a <section> semantic element with props and child VNodes
            return VNode::el("section", std::move(props), std::move(children));
        }

        VNode ul(Props props, std::vector<VNode> children) { // Create an unordered list <ul> element with props and child VNodes
            return VNode::el("ul", std::move(props), std::move(children));
        }

        VNode li(Props props, std::vector<VNode> children) { // Create a list item <li> element with props and child VNodes
            return VNode::el("li", std::move(props), std::move(children));
        }

        VNode form(Props props, std::vector<VNode> children) { // Create a <form> element with props and child VNodes
            return VNode::el("form", std::move(props), std::move(children));
        }

        VNode h1(Props props, std::string text) { // Create an <h1> heading element containing a single text node
            return VNode::el("h1", std::move(props), {VNode::textNode(std::move(text))});
        }

        VNode h2(Props props, std::string text) { // Create an <h2> heading element containing a single text node
            return VNode::el("h2", std::move(props), {VNode::textNode(std::move(text))});
        }

        VNode h3(Props props, std::string text) { // Create an <h3> heading element containing a single text node
            return VNode::el("h3", std::move(props), {VNode::textNode(std::move(text))});
        }

        VNode p(Props props, std::string text) { // Create a <p> paragraph element containing a single text node
            return VNode::el("p", std::move(props), {VNode::textNode(std::move(text))});
        }

        VNode button(Props props, std::string text) { // Create a <button> element containing a single text label
            return VNode::el("button", std::move(props), {VNode::textNode(std::move(text))});
        }

        VNode input(Props props) { // Create a self-closing <input> element with the given attributes
            return VNode::el("input", std::move(props), {});
        }
    }
}
