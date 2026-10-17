//
// Created by PAWEL on 17/10/2026.
//

#ifndef LORIKEET_UIENGINE_H
#define LORIKEET_UIENGINE_H

#include <concepts>
#include <functional>
#include <map>
#include <string>
#include <utility>
#include <vector>

#include "Json.h"
#include "Server.h"

namespace UIEngine {
    using Props = std::map<std::string, std::string>; // Key-value map representing HTML attributes, styles, and declarative data bindings
    using StyleDeclarations = std::map<std::string, std::string>; // Key-value map of CSS property names to values (e.g., {"background", "#1e293b"})

    class StyleSheet { // CSS-in-C++ stylesheet builder that stores selector rules and serializes them into a CSS document
        public:
            StyleSheet() = default; // Default constructor creating an empty stylesheet

            StyleSheet& rule(std::string selector, StyleDeclarations declarations); // Add or merge CSS declarations for a given CSS selector
            void merge(const StyleSheet& other); // Merge all rules from another StyleSheet into this one
            [[nodiscard]] bool empty() const; // Check whether the stylesheet contains any rules
            [[nodiscard]] std::string toCss() const; // Serialize all rules and declarations into a valid CSS string

        private:
            std::vector<std::pair<std::string, StyleDeclarations>> _rules; // Ordered list of CSS selector rules preserving insertion order
    };

    struct VNode { // Virtual DOM node representing either an HTML element or a raw text node
        std::string tag; // HTML tag name (e.g., "div", "button") or "#text" for raw text nodes
        Props props; // Map of HTML attributes and properties attached to this element
        std::string text; // Text content used when tag is "#text"
        std::vector<VNode> children; // Ordered list of child Virtual DOM nodes

        static VNode textNode(std::string content); // Create a raw text VNode with tag "#text" and the given text content
        static VNode el(std::string tag, Props props = {}, std::vector<VNode> children = {}); // Create an HTML element VNode with tag, attributes, and child nodes

        [[nodiscard]] HTTPServer::Json toJson() const; // Recursively serialize this VNode and its children into an HTTPServer::Json object
    };

    class Component { // Abstract base class for object-oriented UI components with props and a pure virtual render() method
        public:
            explicit Component(Props props = {}) : _props(std::move(props)) {} // Construct a component with an optional map of properties
            virtual ~Component() = default; // Virtual destructor for safe polymorphic cleanup of derived components

            [[nodiscard]] virtual VNode render() const = 0; // Pure virtual method implemented by derived components to return their VNode subtree
            [[nodiscard]] virtual StyleSheet styles() const { return {}; } // Optional virtual method allowing components to define their own scoped/associated CSS rules
            operator VNode() const { return render(); } // Implicitly convert any Component instance into its rendered VNode for seamless nesting in children lists

        protected:
            Props _props; // Component properties passed in at construction time

            [[nodiscard]] bool hasProp(const std::string& key) const; // Check whether a specific property key was provided to this component
            [[nodiscard]] std::string getProp(const std::string& key, const std::string& fallback = "") const; // Retrieve a property value by key, or return fallback if absent
    };

    template <typename CompT, typename... Args>
    requires std::derived_from<CompT, Component>
    VNode component(Args&&... args) { // Helper to instantiate a derived Component with arguments and return its rendered VNode
        CompT instance(std::forward<Args>(args)...); // Construct the component instance with forwarded arguments
        return instance.render(); // Render and return the resulting VNode subtree
    }

    // Mount a root VNode factory and StyleSheet onto the frontend HTTP server (registers GET "/", GET "/styles.css", and GET "/vdom")
    void serveApp(
        HTTPServer::Server& frontend,
        std::function<VNode()> rootFactory,
        StyleSheet styleSheet = {},
        std::string apiBaseUrl = "http://localhost:8000",
        std::string pageTitle = "App"
    );

    // Overload of serveApp that accepts a root Component subclass directly and automatically extracts its styles() if no custom StyleSheet is passed
    template <typename CompT>
    requires std::derived_from<CompT, Component>
    void serveApp(
        HTTPServer::Server& frontend,
        CompT rootComponent,
        StyleSheet styleSheet = {},
        std::string apiBaseUrl = "http://localhost:8000",
        std::string pageTitle = "App"
    ) {
        StyleSheet combinedStyles = rootComponent.styles(); // Start with styles declared on the root component
        combinedStyles.merge(styleSheet); // Merge any additional global stylesheet rules passed to serveApp
        serveApp(
            frontend,
            [comp = std::move(rootComponent)]() { return comp.render(); },
            std::move(combinedStyles),
            std::move(apiBaseUrl),
            std::move(pageTitle)
        );
    }

    namespace Tags { // Ergonomic HTML tag helper functions for building VNode trees
        VNode div(Props props = {}, std::vector<VNode> children = {}); // Create a <div> container element with props and child VNodes
        VNode span(Props props = {}, std::vector<VNode> children = {}); // Create a <span> inline container element with props and child VNodes
        VNode span(Props props, const std::string &text); // Create a <span> inline element containing a single text node
        VNode header(Props props = {}, std::vector<VNode> children = {}); // Create a <header> semantic element with props and child VNodes
        VNode main(Props props = {}, std::vector<VNode> children = {}); // Create a <main> semantic element with props and child VNodes
        VNode section(Props props = {}, std::vector<VNode> children = {}); // Create a <section> semantic element with props and child VNodes
        VNode ul(Props props = {}, std::vector<VNode> children = {}); // Create an unordered list <ul> element with props and child VNodes
        VNode li(Props props = {}, std::vector<VNode> children = {}); // Create a list item <li> element with props and child VNodes
        VNode form(Props props = {}, std::vector<VNode> children = {}); // Create a <form> element with props and child VNodes

        VNode h1(Props props, std::string text); // Create an <h1> heading element containing a single text node
        VNode h2(Props props, std::string text); // Create an <h2> heading element containing a single text node
        VNode h3(Props props, std::string text); // Create an <h3> heading element containing a single text node
        VNode p(Props props, std::string text); // Create a <p> paragraph element containing a single text node
        VNode button(Props props, std::string text); // Create a <button> element containing a single text label
        VNode input(Props props = {}); // Create a self-closing <input> element with the given attributes
    }
}

#endif // LORIKEET_UIENGINE_H
