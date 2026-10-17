//
// Created by PAWEL on 17/10/2026.
//

#include "UIEngine.h"

#include <utility>

namespace UIEngine {
    VNode VNode::textNode(std::string content) { // Create a raw text VNode with tag "#text" and the given text content
        return VNode{.tag = "#text", .props = {}, .text = std::move(content), .children = {}}; // Initialize VNode with "#text" tag, empty props, moved text, and no children
    }

    VNode VNode::el(std::string tag, Props props, std::vector<VNode> children) { // Create an HTML element VNode with tag, attributes, and child nodes
        return VNode{.tag = std::move(tag), .props = std::move(props), .text = "", .children = std::move(children)}; // Initialize VNode with moved tag, props, empty text, and moved children
    }

    namespace Tags {
        VNode div(Props props, std::vector<VNode> children) { // Create a <div> container element with props and child VNodes
            return VNode::el("div", std::move(props), std::move(children));
        }

        VNode span(Props props, std::vector<VNode> children) { // Create a <span> inline container element with props and child VNodes
            return VNode::el("span", std::move(props), std::move(children));
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
