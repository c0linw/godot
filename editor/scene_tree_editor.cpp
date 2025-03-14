/*************************************************************************/
/*  scene_tree_editor.cpp                                                */
/*************************************************************************/
/*                       This file is part of:                           */
/*                           GODOT ENGINE                                */
/*                      https://godotengine.org                          */
/*************************************************************************/
/* Copyright (c) 2007-2022 Juan Linietsky, Ariel Manzur.                 */
/* Copyright (c) 2014-2022 Godot Engine contributors (cf. AUTHORS.md).   */
/*                                                                       */
/* Permission is hereby granted, free of charge, to any person obtaining */
/* a copy of this software and associated documentation files (the       */
/* "Software"), to deal in the Software without restriction, including   */
/* without limitation the rights to use, copy, modify, merge, publish,   */
/* distribute, sublicense, and/or sell copies of the Software, and to    */
/* permit persons to whom the Software is furnished to do so, subject to */
/* the following conditions:                                             */
/*                                                                       */
/* The above copyright notice and this permission notice shall be        */
/* included in all copies or substantial portions of the Software.       */
/*                                                                       */
/* THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND,       */
/* EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF    */
/* MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.*/
/* IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY  */
/* CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT,  */
/* TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE     */
/* SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.                */
/*************************************************************************/

#include "scene_tree_editor.h"

#include "core/object/message_queue.h"
#include "core/string/print_string.h"
#include "editor/editor_file_system.h"
#include "editor/editor_node.h"
#include "editor/editor_scale.h"
#include "editor/editor_undo_redo_manager.h"
#include "editor/node_dock.h"
#include "editor/plugins/animation_player_editor_plugin.h"
#include "editor/plugins/canvas_item_editor_plugin.h"
#include "editor/plugins/script_editor_plugin.h"
#include "scene/gui/label.h"
#include "scene/main/window.h"
#include "scene/resources/packed_scene.h"

Node *SceneTreeEditor::get_scene_node() {
	ERR_FAIL_COND_V(!is_inside_tree(), nullptr);

	return get_tree()->get_edited_scene_root();
}

void SceneTreeEditor::_cell_button_pressed(Object *p_item, int p_column, int p_id, MouseButton p_button) {
	if (p_button != MouseButton::LEFT) {
		return;
	}

	if (connect_to_script_mode) {
		return; //don't do anything in this mode
	}

	TreeItem *item = Object::cast_to<TreeItem>(p_item);
	ERR_FAIL_COND(!item);

	NodePath np = item->get_metadata(0);

	Node *n = get_node(np);
	ERR_FAIL_COND(!n);

	if (p_id == BUTTON_SUBSCENE) {
		if (n == get_scene_node()) {
			if (n && n->get_scene_inherited_state().is_valid()) {
				emit_signal(SNAME("open"), n->get_scene_inherited_state()->get_path());
			}
		} else {
			emit_signal(SNAME("open"), n->get_scene_file_path());
		}
	} else if (p_id == BUTTON_SCRIPT) {
		Ref<Script> script_typed = n->get_script();
		if (!script_typed.is_null()) {
			emit_signal(SNAME("open_script"), script_typed);
		}

	} else if (p_id == BUTTON_VISIBILITY) {
		undo_redo->create_action(TTR("Toggle Visible"));
		_toggle_visible(n);
		List<Node *> selection = editor_selection->get_selected_node_list();
		if (selection.size() > 1 && selection.find(n) != nullptr) {
			for (Node *nv : selection) {
				ERR_FAIL_COND(!nv);
				if (nv == n) {
					continue;
				}
				_toggle_visible(nv);
			}
		}
		undo_redo->commit_action();
	} else if (p_id == BUTTON_LOCK) {
		undo_redo->create_action(TTR("Unlock Node"));

		if (n->is_class("CanvasItem") || n->is_class("Node3D")) {
			undo_redo->add_do_method(n, "remove_meta", "_edit_lock_");
			undo_redo->add_undo_method(n, "set_meta", "_edit_lock_", true);
			undo_redo->add_do_method(this, "_update_tree");
			undo_redo->add_undo_method(this, "_update_tree");
			undo_redo->add_do_method(this, "emit_signal", "node_changed");
			undo_redo->add_undo_method(this, "emit_signal", "node_changed");
		}
		undo_redo->commit_action();
	} else if (p_id == BUTTON_PIN) {
		if (n->is_class("AnimationPlayer")) {
			AnimationPlayerEditor::get_singleton()->unpin();
			_update_tree();
		}

	} else if (p_id == BUTTON_GROUP) {
		undo_redo->create_action(TTR("Button Group"));

		if (n->is_class("CanvasItem") || n->is_class("Node3D")) {
			undo_redo->add_do_method(n, "remove_meta", "_edit_group_");
			undo_redo->add_undo_method(n, "set_meta", "_edit_group_", true);
			undo_redo->add_do_method(this, "_update_tree");
			undo_redo->add_undo_method(this, "_update_tree");
			undo_redo->add_do_method(this, "emit_signal", "node_changed");
			undo_redo->add_undo_method(this, "emit_signal", "node_changed");
		}
		undo_redo->commit_action();
	} else if (p_id == BUTTON_WARNING) {
		String config_err = n->get_configuration_warnings_as_string();
		if (config_err.is_empty()) {
			return;
		}
		config_err = config_err.word_wrap(80);
		warning->set_text(config_err);
		warning->popup_centered();

	} else if (p_id == BUTTON_SIGNALS) {
		editor_selection->clear();
		editor_selection->add_node(n);

		set_selected(n);

		TabContainer *tab_container = Object::cast_to<TabContainer>(NodeDock::get_singleton()->get_parent());
		NodeDock::get_singleton()->get_parent()->call("set_current_tab", tab_container->get_tab_idx_from_control(NodeDock::get_singleton()));
		NodeDock::get_singleton()->show_connections();

	} else if (p_id == BUTTON_GROUPS) {
		editor_selection->clear();
		editor_selection->add_node(n);

		set_selected(n);

		TabContainer *tab_container = Object::cast_to<TabContainer>(NodeDock::get_singleton()->get_parent());
		NodeDock::get_singleton()->get_parent()->call("set_current_tab", tab_container->get_tab_idx_from_control(NodeDock::get_singleton()));
		NodeDock::get_singleton()->show_groups();
	} else if (p_id == BUTTON_UNIQUE) {
		undo_redo->create_action(TTR("Disable Scene Unique Name"));
		undo_redo->add_do_method(n, "set_unique_name_in_owner", false);
		undo_redo->add_undo_method(n, "set_unique_name_in_owner", true);
		undo_redo->add_do_method(this, "_update_tree");
		undo_redo->add_undo_method(this, "_update_tree");
		undo_redo->commit_action();
	}
}

void SceneTreeEditor::_toggle_visible(Node *p_node) {
	if (p_node->has_method("is_visible") && p_node->has_method("set_visible")) {
		bool v = bool(p_node->call("is_visible"));
		undo_redo->add_do_method(p_node, "set_visible", !v);
		undo_redo->add_undo_method(p_node, "set_visible", v);
	}
}

void SceneTreeEditor::_add_nodes(Node *p_node, TreeItem *p_parent) {
	if (!p_node) {
		return;
	}

	// only owned nodes are editable, since nodes can create their own (manually owned) child nodes,
	// which the editor needs not to know about.

	bool part_of_subscene = false;

	if (!display_foreign && p_node->get_owner() != get_scene_node() && p_node != get_scene_node()) {
		if ((show_enabled_subscene || can_open_instance) && p_node->get_owner() && (get_scene_node()->is_editable_instance(p_node->get_owner()))) {
			part_of_subscene = true;
			//allow
		} else {
			return;
		}
	} else {
		part_of_subscene = p_node != get_scene_node() && get_scene_node()->get_scene_inherited_state().is_valid() && get_scene_node()->get_scene_inherited_state()->find_node_by_path(get_scene_node()->get_path_to(p_node)) >= 0;
	}

	TreeItem *item = tree->create_item(p_parent);

	item->set_text(0, p_node->get_name());
	if (can_rename && !part_of_subscene) {
		item->set_editable(0, true);
	}

	item->set_selectable(0, true);
	if (can_rename) {
		bool collapsed = p_node->is_displayed_folded();
		if (collapsed) {
			item->set_collapsed(true);
		}
	}

	Ref<Texture2D> icon = EditorNode::get_singleton()->get_object_icon(p_node, "Node");
	item->set_icon(0, icon);
	item->set_metadata(0, p_node->get_path());

	if (connect_to_script_mode) {
		Color accent = get_theme_color(SNAME("accent_color"), SNAME("Editor"));

		Ref<Script> script = p_node->get_script();
		if (!script.is_null() && EditorNode::get_singleton()->get_object_custom_type_base(p_node) != script) {
			//has script
			item->add_button(0, get_theme_icon(SNAME("Script"), SNAME("EditorIcons")), BUTTON_SCRIPT);
		} else {
			//has no script (or script is a custom type)
			item->set_custom_color(0, get_theme_color(SNAME("disabled_font_color"), SNAME("Editor")));
			item->set_selectable(0, false);

			if (!script.is_null()) { // make sure to mark the script if a custom type
				item->add_button(0, get_theme_icon(SNAME("Script"), SNAME("EditorIcons")), BUTTON_SCRIPT);
				item->set_button_disabled(0, item->get_button_count(0) - 1, true);
			}

			accent.a *= 0.7;
		}

		if (marked.has(p_node)) {
			String node_name = p_node->get_name();
			if (connecting_signal) {
				node_name += " " + TTR("(Connecting From)");
			}
			item->set_text(0, node_name);
			item->set_custom_color(0, accent);
		}
	} else if (part_of_subscene) {
		if (valid_types.size() == 0) {
			item->set_custom_color(0, get_theme_color(SNAME("warning_color"), SNAME("Editor")));
		}
	} else if (marked.has(p_node)) {
		String node_name = p_node->get_name();
		if (connecting_signal) {
			node_name += " " + TTR("(Connecting From)");
		}
		item->set_text(0, node_name);
		item->set_selectable(0, marked_selectable);
		item->set_custom_color(0, get_theme_color(SNAME("accent_color"), SNAME("Editor")));
	} else if (!p_node->can_process()) {
		item->set_custom_color(0, get_theme_color(SNAME("disabled_font_color"), SNAME("Editor")));
	} else if (!marked_selectable && !marked_children_selectable) {
		Node *node = p_node;
		while (node) {
			if (marked.has(node)) {
				item->set_selectable(0, false);
				item->set_custom_color(0, get_theme_color(SNAME("error_color"), SNAME("Editor")));
				break;
			}
			node = node->get_parent();
		}
	}

	if (can_rename) { //should be can edit..

		String warning = p_node->get_configuration_warnings_as_string();
		if (!warning.is_empty()) {
			const int num_warnings = p_node->get_configuration_warnings().size();
			String warning_icon;
			if (num_warnings == 1) {
				warning_icon = SNAME("NodeWarning");
			} else if (num_warnings <= 3) {
				warning_icon = vformat("NodeWarnings%d", num_warnings);
			} else {
				warning_icon = SNAME("NodeWarnings4Plus");
			}

			// Improve looks on tooltip, extra spacing on non-bullet point newlines.
			const String bullet_point = String::utf8("•  ");
			int next_newline = 0;
			while (next_newline != -1) {
				next_newline = warning.find("\n", next_newline + 2);
				if (warning.substr(next_newline + 1, bullet_point.length()) != bullet_point) {
					warning = warning.insert(next_newline + 1, "    ");
				}
			}

			String newline = (num_warnings == 1 ? "\n" : "\n\n");

			item->add_button(0, get_theme_icon(warning_icon, SNAME("EditorIcons")), BUTTON_WARNING, false, TTR("Node configuration warning:") + newline + warning);
		}

		if (p_node->is_unique_name_in_owner()) {
			item->add_button(0, get_theme_icon(SNAME("SceneUniqueName"), SNAME("EditorIcons")), BUTTON_UNIQUE, false, vformat(TTR("This node can be accessed from within anywhere in the scene by preceding it with the '%s' prefix in a node path.\nClick to disable this."), UNIQUE_NODE_PREFIX));
		}

		int num_connections = p_node->get_persistent_signal_connection_count();
		int num_groups = p_node->get_persistent_group_count();

		String msg_temp;
		if (num_connections >= 1) {
			Array arr;
			arr.push_back(num_connections);
			msg_temp += TTRN("Node has one connection.", "Node has {num} connections.", num_connections).format(arr, "{num}");
			if (num_groups >= 1) {
				msg_temp += "\n";
			}
		}
		if (num_groups >= 1) {
			msg_temp += TTRN("Node is in this group:", "Node is in the following groups:", num_groups) + "\n";

			List<GroupInfo> groups;
			p_node->get_groups(&groups);
			for (const GroupInfo &E : groups) {
				if (E.persistent) {
					msg_temp += String::utf8("•  ") + String(E.name) + "\n";
				}
			}
		}
		if (num_connections >= 1 || num_groups >= 1) {
			if (num_groups < 1) {
				msg_temp += "\n";
			}
			msg_temp += TTR("Click to show signals dock.");
		}

		Ref<Texture2D> icon_temp;
		SceneTreeEditorButton signal_temp = BUTTON_SIGNALS;
		if (num_connections >= 1 && num_groups >= 1) {
			icon_temp = get_theme_icon(SNAME("SignalsAndGroups"), SNAME("EditorIcons"));
		} else if (num_connections >= 1) {
			icon_temp = get_theme_icon(SNAME("Signals"), SNAME("EditorIcons"));
		} else if (num_groups >= 1) {
			icon_temp = get_theme_icon(SNAME("Groups"), SNAME("EditorIcons"));
			signal_temp = BUTTON_GROUPS;
		}

		if (num_connections >= 1 || num_groups >= 1) {
			item->add_button(0, icon_temp, signal_temp, false, msg_temp);
		}
	}

	// Display the node name in all tooltips so that long node names can be previewed
	// without having to rename them.
	if (p_node == get_scene_node() && p_node->get_scene_inherited_state().is_valid()) {
		item->add_button(0, get_theme_icon(SNAME("InstanceOptions"), SNAME("EditorIcons")), BUTTON_SUBSCENE, false, TTR("Open in Editor"));

		String tooltip = String(p_node->get_name()) + "\n" + TTR("Inherits:") + " " + p_node->get_scene_inherited_state()->get_path() + "\n" + TTR("Type:") + " " + p_node->get_class();
		if (!p_node->get_editor_description().is_empty()) {
			tooltip += "\n\n" + p_node->get_editor_description();
		}

		item->set_tooltip_text(0, tooltip);
	} else if (p_node != get_scene_node() && !p_node->get_scene_file_path().is_empty() && can_open_instance) {
		item->add_button(0, get_theme_icon(SNAME("InstanceOptions"), SNAME("EditorIcons")), BUTTON_SUBSCENE, false, TTR("Open in Editor"));

		String tooltip = String(p_node->get_name()) + "\n" + TTR("Instance:") + " " + p_node->get_scene_file_path() + "\n" + TTR("Type:") + " " + p_node->get_class();
		if (!p_node->get_editor_description().is_empty()) {
			tooltip += "\n\n" + p_node->get_editor_description();
		}

		item->set_tooltip_text(0, tooltip);
	} else {
		StringName type = EditorNode::get_singleton()->get_object_custom_type_name(p_node);
		if (type == StringName()) {
			type = p_node->get_class();
		}

		String tooltip = String(p_node->get_name()) + "\n" + TTR("Type:") + " " + type;
		if (!p_node->get_editor_description().is_empty()) {
			tooltip += "\n\n" + p_node->get_editor_description();
		}

		item->set_tooltip_text(0, tooltip);
	}

	if (can_open_instance && undo_redo.is_valid()) { //Show buttons only when necessary(SceneTreeDock) to avoid crashes

		if (!p_node->is_connected("script_changed", callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
			p_node->connect("script_changed", callable_mp(this, &SceneTreeEditor::_node_script_changed).bind(p_node));
		}

		Ref<Script> script = p_node->get_script();
		if (!script.is_null()) {
			String additional_notes;
			Color button_color = Color(1, 1, 1);
			// Can't set tooltip after adding button, need to do it before.
			if (script->is_tool()) {
				additional_notes += "\n" + TTR("This script is currently running in the editor.");
				button_color = get_theme_color(SNAME("accent_color"), SNAME("Editor"));
			}
			if (EditorNode::get_singleton()->get_object_custom_type_base(p_node) == script) {
				additional_notes += "\n" + TTR("This script is a custom type.");
				button_color.a = 0.5;
			}
			item->add_button(0, get_theme_icon(SNAME("Script"), SNAME("EditorIcons")), BUTTON_SCRIPT, false, TTR("Open Script:") + " " + script->get_path() + additional_notes);
			item->set_button_color(0, item->get_button_count(0) - 1, button_color);
		}

		if (p_node->is_class("CanvasItem")) {
			if (p_node->has_meta("_edit_lock_")) {
				item->add_button(0, get_theme_icon(SNAME("Lock"), SNAME("EditorIcons")), BUTTON_LOCK, false, TTR("Node is locked.\nClick to unlock it."));
			}

			if (p_node->has_meta("_edit_group_")) {
				item->add_button(0, get_theme_icon(SNAME("Group"), SNAME("EditorIcons")), BUTTON_GROUP, false, TTR("Children are not selectable.\nClick to make them selectable."));
			}

			bool v = p_node->call("is_visible");
			if (v) {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityVisible"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			} else {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityHidden"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			}

			if (!p_node->is_connected("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed))) {
				p_node->connect("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed).bind(p_node));
			}

			_update_visibility_color(p_node, item);
		} else if (p_node->is_class("CanvasLayer") || p_node->is_class("Window")) {
			bool v = p_node->call("is_visible");
			if (v) {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityVisible"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			} else {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityHidden"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			}

			if (!p_node->is_connected("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed))) {
				p_node->connect("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed).bind(p_node));
			}
		} else if (p_node->is_class("Node3D")) {
			if (p_node->has_meta("_edit_lock_")) {
				item->add_button(0, get_theme_icon(SNAME("Lock"), SNAME("EditorIcons")), BUTTON_LOCK, false, TTR("Node is locked.\nClick to unlock it."));
			}

			if (p_node->has_meta("_edit_group_")) {
				item->add_button(0, get_theme_icon(SNAME("Group"), SNAME("EditorIcons")), BUTTON_GROUP, false, TTR("Children are not selectable.\nClick to make them selectable."));
			}

			bool v = p_node->call("is_visible");
			if (v) {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityVisible"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			} else {
				item->add_button(0, get_theme_icon(SNAME("GuiVisibilityHidden"), SNAME("EditorIcons")), BUTTON_VISIBILITY, false, TTR("Toggle Visibility"));
			}

			if (!p_node->is_connected("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed))) {
				p_node->connect("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed).bind(p_node));
			}

			_update_visibility_color(p_node, item);
		} else if (p_node->is_class("AnimationPlayer")) {
			bool is_pinned = AnimationPlayerEditor::get_singleton()->get_player() == p_node && AnimationPlayerEditor::get_singleton()->is_pinned();

			if (is_pinned) {
				item->add_button(0, get_theme_icon(SNAME("Pin"), SNAME("EditorIcons")), BUTTON_PIN, false, TTR("AnimationPlayer is pinned.\nClick to unpin."));
			}
		}
	}

	if (editor_selection) {
		if (editor_selection->is_selected(p_node)) {
			item->select(0);
		}
	}

	if (selected == p_node) {
		if (!editor_selection) {
			item->select(0);
		}
		item->set_as_cursor(0);
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_add_nodes(p_node->get_child(i), item);
	}

	if (valid_types.size()) {
		bool valid = false;
		for (int i = 0; i < valid_types.size(); i++) {
			if (p_node->is_class(valid_types[i])) {
				valid = true;
				break;
			}
		}

		if (!valid) {
			item->set_custom_color(0, get_theme_color(SNAME("disabled_font_color"), SNAME("Editor")));
			item->set_selectable(0, false);
		}
	}
}

void SceneTreeEditor::_node_visibility_changed(Node *p_node) {
	if (!p_node || (p_node != get_scene_node() && !p_node->get_owner())) {
		return;
	}

	TreeItem *item = _find(tree->get_root(), p_node->get_path());

	if (!item) {
		return;
	}

	int idx = item->get_button_by_id(0, BUTTON_VISIBILITY);
	ERR_FAIL_COND(idx == -1);

	bool visible = false;

	if (p_node->is_class("CanvasItem") || p_node->is_class("CanvasLayer") || p_node->is_class("Window")) {
		visible = p_node->call("is_visible");
		CanvasItemEditor::get_singleton()->get_viewport_control()->queue_redraw();
	} else if (p_node->is_class("Node3D")) {
		visible = p_node->call("is_visible");
	}

	if (visible) {
		item->set_button(0, idx, get_theme_icon(SNAME("GuiVisibilityVisible"), SNAME("EditorIcons")));
	} else {
		item->set_button(0, idx, get_theme_icon(SNAME("GuiVisibilityHidden"), SNAME("EditorIcons")));
	}

	_update_visibility_color(p_node, item);
}

void SceneTreeEditor::_update_visibility_color(Node *p_node, TreeItem *p_item) {
	if (p_node->is_class("CanvasItem") || p_node->is_class("Node3D")) {
		Color color(1, 1, 1, 1);
		bool visible_on_screen = p_node->call("is_visible_in_tree");
		if (!visible_on_screen) {
			color.a = 0.6;
		}
		int idx = p_item->get_button_by_id(0, BUTTON_VISIBILITY);
		p_item->set_button_color(0, idx, color);
	}
}

void SceneTreeEditor::_node_script_changed(Node *p_node) {
	if (tree_dirty) {
		return;
	}

	MessageQueue::get_singleton()->push_call(this, "_update_tree");
	tree_dirty = true;
}

void SceneTreeEditor::_node_removed(Node *p_node) {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; //speed up exit
	}

	if (p_node->is_connected("script_changed", callable_mp(this, &SceneTreeEditor::_node_script_changed))) {
		p_node->disconnect("script_changed", callable_mp(this, &SceneTreeEditor::_node_script_changed));
	}

	if (p_node->is_class("Node3D") || p_node->is_class("CanvasItem") || p_node->is_class("CanvasLayer") || p_node->is_class("Window")) {
		if (p_node->is_connected("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed))) {
			p_node->disconnect("visibility_changed", callable_mp(this, &SceneTreeEditor::_node_visibility_changed));
		}
	}

	if (p_node == selected) {
		selected = nullptr;
		emit_signal(SNAME("node_selected"));
	}
}

void SceneTreeEditor::_node_renamed(Node *p_node) {
	if (p_node != get_scene_node() && !get_scene_node()->is_ancestor_of(p_node)) {
		return;
	}

	emit_signal(SNAME("node_renamed"));

	if (!tree_dirty) {
		MessageQueue::get_singleton()->push_call(this, "_update_tree");
		tree_dirty = true;
	}
}

void SceneTreeEditor::_update_tree(bool p_scroll_to_selected) {
	if (!is_inside_tree()) {
		tree_dirty = false;
		return;
	}

	if (tree->is_editing()) {
		return;
	}

	updating_tree = true;
	tree->clear();
	if (get_scene_node()) {
		_add_nodes(get_scene_node(), nullptr);
		last_hash = hash_djb2_one_64(0);
		_compute_hash(get_scene_node(), last_hash);
	}
	updating_tree = false;
	tree_dirty = false;

	if (!filter.strip_edges().is_empty()) {
		_update_filter(nullptr, p_scroll_to_selected);
	}
}

bool SceneTreeEditor::_update_filter(TreeItem *p_parent, bool p_scroll_to_selected) {
	if (!p_parent) {
		p_parent = tree->get_root();
	}

	if (!p_parent) {
		// Tree is empty, nothing to do here.
		return false;
	}

	bool keep_for_children = false;
	for (TreeItem *child = p_parent->get_first_child(); child; child = child->get_next()) {
		// Always keep if at least one of the children are kept.
		keep_for_children = _update_filter(child, p_scroll_to_selected) || keep_for_children;
	}

	// Now find other reasons to keep this Node, too.
	PackedStringArray terms = filter.to_lower().split_spaces();
	bool keep = _item_matches_all_terms(p_parent, terms);

	p_parent->set_visible(keep_for_children || keep);
	if (keep_for_children) {
		if (keep) {
			p_parent->clear_custom_color(0);
			p_parent->set_selectable(0, true);
		} else {
			p_parent->set_custom_color(0, get_theme_color(SNAME("disabled_font_color"), SNAME("Editor")));
			p_parent->set_selectable(0, false);
			p_parent->deselect(0);
		}
	}

	if (editor_selection) {
		Node *n = get_node(p_parent->get_metadata(0));
		if (keep) {
			if (p_scroll_to_selected && n && editor_selection->is_selected(n)) {
				tree->scroll_to_item(p_parent);
			}
		} else {
			if (n && p_parent->is_selected(0)) {
				editor_selection->remove_node(n);
				p_parent->deselect(0);
			}
		}
	}

	return keep || keep_for_children;
}

bool SceneTreeEditor::_item_matches_all_terms(TreeItem *p_item, PackedStringArray p_terms) {
	if (p_terms.is_empty()) {
		return true;
	}

	for (int i = 0; i < p_terms.size(); i++) {
		String term = p_terms[i];

		// Recognise special filter.
		if (term.contains(":") && !term.get_slicec(':', 0).is_empty()) {
			String parameter = term.get_slicec(':', 0);
			String argument = term.get_slicec(':', 1);

			if (parameter == "type" || parameter == "t") {
				// Filter by Type.
				String node_type = get_node(p_item->get_metadata(0))->get_class().to_lower();

				if (!node_type.contains(argument)) {
					return false;
				}
			} else if (parameter == "group" || parameter == "g") {
				// Filter by Group.
				Node *node = get_node(p_item->get_metadata(0));

				List<Node::GroupInfo> group_info_list;
				node->get_groups(&group_info_list);
				if (group_info_list.is_empty()) {
					return false;
				}
				// When argument is empty, match all Nodes belonging to any group.
				if (!argument.is_empty()) {
					bool term_in_groups = false;
					for (int j = 0; j < group_info_list.size(); j++) {
						// Ignore private groups.
						if (String(group_info_list[j].name).begins_with("__")) {
							continue;
						}
						if (String(group_info_list[j].name).to_lower().contains(argument)) {
							term_in_groups = true;
							break;
						}
					}
					if (!term_in_groups) {
						return false;
					}
				}
			} else {
				WARN_PRINT(vformat(TTR("Special Node filter \"%s\" is not recognised. Available filters include \"type\" and \"group\"."), parameter));
				continue;
			}
		} else {
			// Default.
			if (!p_item->get_text(0).to_lower().contains(term)) {
				return false;
			}
		}
	}

	return true;
}

void SceneTreeEditor::_compute_hash(Node *p_node, uint64_t &hash) {
	hash = hash_djb2_one_64(p_node->get_instance_id(), hash);
	if (p_node->get_parent()) {
		hash = hash_djb2_one_64(p_node->get_parent()->get_instance_id(), hash); //so a reparent still produces a different hash
	}

	for (int i = 0; i < p_node->get_child_count(); i++) {
		_compute_hash(p_node->get_child(i), hash);
	}
}

void SceneTreeEditor::_test_update_tree() {
	pending_test_update = false;

	if (!is_inside_tree()) {
		return;
	}

	if (tree_dirty) {
		return; // don't even bother
	}

	uint64_t hash = hash_djb2_one_64(0);
	if (get_scene_node()) {
		_compute_hash(get_scene_node(), hash);
	}
	//test hash
	if (hash == last_hash) {
		return; // did not change
	}

	MessageQueue::get_singleton()->push_call(this, "_update_tree");
	tree_dirty = true;
}

void SceneTreeEditor::_tree_process_mode_changed() {
	MessageQueue::get_singleton()->push_call(this, "_update_tree");
	tree_dirty = true;
}

void SceneTreeEditor::_tree_changed() {
	if (EditorNode::get_singleton()->is_exiting()) {
		return; //speed up exit
	}
	if (pending_test_update) {
		return;
	}
	if (tree_dirty) {
		return;
	}

	MessageQueue::get_singleton()->push_call(this, "_test_update_tree");
	pending_test_update = true;
}

void SceneTreeEditor::_selected_changed() {
	TreeItem *s = tree->get_selected();
	ERR_FAIL_COND(!s);
	NodePath np = s->get_metadata(0);

	Node *n = get_node(np);

	if (n == selected) {
		return;
	}

	selected = get_node(np);

	blocked++;
	emit_signal(SNAME("node_selected"));
	blocked--;
}

void SceneTreeEditor::_deselect_items() {
	// Clear currently selected items in scene tree dock.
	if (editor_selection) {
		editor_selection->clear();
		emit_signal(SNAME("node_changed"));
	}
}

void SceneTreeEditor::_cell_multi_selected(Object *p_object, int p_cell, bool p_selected) {
	TreeItem *item = Object::cast_to<TreeItem>(p_object);
	ERR_FAIL_COND(!item);

	NodePath np = item->get_metadata(0);

	Node *n = get_node(np);

	if (!n) {
		return;
	}

	if (!editor_selection) {
		return;
	}

	if (p_selected) {
		editor_selection->add_node(n);

	} else {
		editor_selection->remove_node(n);
	}

	// Emitted "selected" in _selected_changed() when select single node, so select multiple node emit "changed"
	if (editor_selection->get_selected_nodes().size() > 1) {
		emit_signal(SNAME("node_changed"));
	}
}

void SceneTreeEditor::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_ENTER_TREE: {
			get_tree()->connect("tree_changed", callable_mp(this, &SceneTreeEditor::_tree_changed));
			get_tree()->connect("tree_process_mode_changed", callable_mp(this, &SceneTreeEditor::_tree_process_mode_changed));
			get_tree()->connect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->connect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			get_tree()->connect("node_configuration_warning_changed", callable_mp(this, &SceneTreeEditor::_warning_changed));

			tree->connect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));

			_update_tree();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			get_tree()->disconnect("tree_changed", callable_mp(this, &SceneTreeEditor::_tree_changed));
			get_tree()->disconnect("tree_process_mode_changed", callable_mp(this, &SceneTreeEditor::_tree_process_mode_changed));
			get_tree()->disconnect("node_removed", callable_mp(this, &SceneTreeEditor::_node_removed));
			get_tree()->disconnect("node_renamed", callable_mp(this, &SceneTreeEditor::_node_renamed));
			tree->disconnect("item_collapsed", callable_mp(this, &SceneTreeEditor::_cell_collapsed));
			get_tree()->disconnect("node_configuration_warning_changed", callable_mp(this, &SceneTreeEditor::_warning_changed));
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_tree();
		} break;
	}
}

TreeItem *SceneTreeEditor::_find(TreeItem *p_node, const NodePath &p_path) {
	if (!p_node) {
		return nullptr;
	}

	NodePath np = p_node->get_metadata(0);
	if (np == p_path) {
		return p_node;
	}

	TreeItem *children = p_node->get_first_child();
	while (children) {
		TreeItem *n = _find(children, p_path);
		if (n) {
			return n;
		}
		children = children->get_next();
	}

	return nullptr;
}

void SceneTreeEditor::set_selected(Node *p_node, bool p_emit_selected) {
	ERR_FAIL_COND(blocked > 0);

	if (pending_test_update) {
		_test_update_tree();
	}
	if (tree_dirty) {
		_update_tree();
	}

	if (selected == p_node) {
		return;
	}

	TreeItem *item = p_node ? _find(tree->get_root(), p_node->get_path()) : nullptr;

	if (item) {
		if (auto_expand_selected) {
			// Make visible when it's collapsed.
			TreeItem *node = item->get_parent();
			while (node && node != tree->get_root()) {
				node->set_collapsed(false);
				node = node->get_parent();
			}
			item->select(0);
			item->set_as_cursor(0);
			selected = p_node;
			tree->ensure_cursor_is_visible();
		}
	} else {
		if (!p_node) {
			selected = nullptr;
		}
		_update_tree();
		selected = p_node;
	}

	if (p_emit_selected) {
		emit_signal(SNAME("node_selected"));
	}
}

void SceneTreeEditor::_rename_node(ObjectID p_node, const String &p_name) {
	Object *o = ObjectDB::get_instance(p_node);
	ERR_FAIL_COND(!o);
	Node *n = Object::cast_to<Node>(o);
	ERR_FAIL_COND(!n);
	TreeItem *item = _find(tree->get_root(), n->get_path());
	ERR_FAIL_COND(!item);

	n->set_name(p_name);
	item->set_metadata(0, n->get_path());
	item->set_text(0, p_name);
}

void SceneTreeEditor::_renamed() {
	TreeItem *which = tree->get_edited();

	ERR_FAIL_COND(!which);
	NodePath np = which->get_metadata(0);
	Node *n = get_node(np);
	ERR_FAIL_COND(!n);

	// Empty node names are not allowed, so resets it to previous text and show warning
	if (which->get_text(0).strip_edges().is_empty()) {
		which->set_text(0, n->get_name());
		EditorNode::get_singleton()->show_warning(TTR("No name provided."));
		return;
	}

	String raw_new_name = which->get_text(0);
	String new_name = raw_new_name.validate_node_name();

	if (new_name != raw_new_name) {
		error->set_text(TTR("Invalid node name, the following characters are not allowed:") + "\n" + String::invalid_node_name_characters);
		error->popup_centered();

		if (new_name.is_empty()) {
			which->set_text(0, n->get_name());
			return;
		}

		which->set_text(0, new_name);
	}

	if (new_name == n->get_name()) {
		return;
	}

	// Trim leading/trailing whitespace to prevent node names from containing accidental whitespace, which would make it more difficult to get the node via `get_node()`.
	new_name = new_name.strip_edges();

	if (n->is_unique_name_in_owner() && get_tree()->get_edited_scene_root()->get_node_or_null("%" + new_name) != nullptr) {
		error->set_text(TTR("Another node already uses this unique name in the scene."));
		error->popup_centered();
		which->set_text(0, n->get_name());
		return;
	}

	if (!undo_redo.is_valid()) {
		n->set_name(new_name);
		which->set_metadata(0, n->get_path());
		emit_signal(SNAME("node_renamed"));
	} else {
		undo_redo->create_action(TTR("Rename Node"));
		emit_signal(SNAME("node_prerename"), n, new_name);
		undo_redo->add_do_method(this, "_rename_node", n->get_instance_id(), new_name);
		undo_redo->add_undo_method(this, "_rename_node", n->get_instance_id(), n->get_name());
		undo_redo->commit_action();
	}
}

Node *SceneTreeEditor::get_selected() {
	return selected;
}

void SceneTreeEditor::set_marked(const HashSet<Node *> &p_marked, bool p_selectable, bool p_children_selectable) {
	if (tree_dirty) {
		_update_tree();
	}
	marked = p_marked;
	marked_selectable = p_selectable;
	marked_children_selectable = p_children_selectable;
	_update_tree();
}

void SceneTreeEditor::set_marked(Node *p_marked, bool p_selectable, bool p_children_selectable) {
	HashSet<Node *> s;
	if (p_marked) {
		s.insert(p_marked);
	}
	set_marked(s, p_selectable, p_children_selectable);
}

void SceneTreeEditor::set_filter(const String &p_filter) {
	filter = p_filter;
	_update_filter(nullptr, true);
}

String SceneTreeEditor::get_filter() const {
	return filter;
}

void SceneTreeEditor::set_undo_redo(Ref<EditorUndoRedoManager> p_undo_redo) {
	undo_redo = p_undo_redo;
}

void SceneTreeEditor::set_display_foreign_nodes(bool p_display) {
	display_foreign = p_display;
	_update_tree();
}

void SceneTreeEditor::set_valid_types(const Vector<StringName> &p_valid) {
	valid_types = p_valid;
}

void SceneTreeEditor::set_editor_selection(EditorSelection *p_selection) {
	editor_selection = p_selection;
	tree->set_select_mode(Tree::SELECT_MULTI);
	tree->set_cursor_can_exit_tree(false);
	editor_selection->connect("selection_changed", callable_mp(this, &SceneTreeEditor::_selection_changed));
}

void SceneTreeEditor::_update_selection(TreeItem *item) {
	ERR_FAIL_COND(!item);

	NodePath np = item->get_metadata(0);

	if (!has_node(np)) {
		return;
	}

	Node *n = get_node(np);

	if (!n) {
		return;
	}

	if (editor_selection->is_selected(n)) {
		item->select(0);
	} else {
		item->deselect(0);
	}

	TreeItem *c = item->get_first_child();

	while (c) {
		_update_selection(c);
		c = c->get_next();
	}
}

void SceneTreeEditor::_selection_changed() {
	if (!editor_selection) {
		return;
	}

	TreeItem *root = tree->get_root();

	if (!root) {
		return;
	}
	_update_selection(root);
}

void SceneTreeEditor::_cell_collapsed(Object *p_obj) {
	if (updating_tree) {
		return;
	}
	if (!can_rename) {
		return;
	}

	TreeItem *ti = Object::cast_to<TreeItem>(p_obj);
	if (!ti) {
		return;
	}

	bool collapsed = ti->is_collapsed();

	NodePath np = ti->get_metadata(0);

	Node *n = get_node(np);
	ERR_FAIL_COND(!n);

	n->set_display_folded(collapsed);
}

Variant SceneTreeEditor::get_drag_data_fw(const Point2 &p_point, Control *p_from) {
	if (!can_rename) {
		return Variant(); //not editable tree
	}

	if (tree->get_button_id_at_position(p_point) != -1) {
		return Variant(); //dragging from button
	}

	Vector<Node *> selected;
	Vector<Ref<Texture2D>> icons;
	TreeItem *next = tree->get_next_selected(nullptr);
	while (next) {
		NodePath np = next->get_metadata(0);

		Node *n = get_node(np);
		if (n) {
			// Only allow selection if not part of an instantiated scene.
			if (!n->get_owner() || n->get_owner() == get_scene_node() || n->get_owner()->get_scene_file_path().is_empty()) {
				selected.push_back(n);
				icons.push_back(next->get_icon(0));
			}
		}
		next = tree->get_next_selected(next);
	}

	if (selected.is_empty()) {
		return Variant();
	}

	VBoxContainer *vb = memnew(VBoxContainer);
	Array objs;
	int list_max = 10;
	float opacity_step = 1.0f / list_max;
	float opacity_item = 1.0f;
	for (int i = 0; i < selected.size(); i++) {
		if (i < list_max) {
			HBoxContainer *hb = memnew(HBoxContainer);
			TextureRect *tf = memnew(TextureRect);
			tf->set_texture(icons[i]);
			tf->set_stretch_mode(TextureRect::STRETCH_KEEP_CENTERED);
			hb->add_child(tf);
			Label *label = memnew(Label(selected[i]->get_name()));
			hb->add_child(label);
			vb->add_child(hb);
			hb->set_modulate(Color(1, 1, 1, opacity_item));
			opacity_item -= opacity_step;
		}
		NodePath p = selected[i]->get_path();
		objs.push_back(p);
	}

	set_drag_preview(vb);
	Dictionary drag_data;
	drag_data["type"] = "nodes";
	drag_data["nodes"] = objs;

	tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
	emit_signal(SNAME("nodes_dragged"));

	return drag_data;
}

bool SceneTreeEditor::_is_script_type(const StringName &p_type) const {
	return (script_types->find(p_type));
}

bool SceneTreeEditor::can_drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) const {
	if (!can_rename) {
		return false; //not editable tree
	}

	Dictionary d = p_data;
	if (!d.has("type")) {
		return false;
	}

	TreeItem *item = tree->get_item_at_position(p_point);
	if (!item) {
		return false;
	}

	int section = tree->get_drop_section_at_position(p_point);
	if (section < -1 || (section == -1 && !item->get_parent())) {
		return false;
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		if (files.size() == 0) {
			return false; //weird
		}

		if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(files[0]))) {
			tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
			return true;
		}

		bool scene_drop = true;
		for (int i = 0; i < files.size(); i++) {
			String file = files[i];
			String ftype = EditorFileSystem::get_singleton()->get_file_type(file);
			if (ftype != "PackedScene") {
				scene_drop = false;
				break;
			}
		}

		if (scene_drop) {
			tree->set_drop_mode_flags(Tree::DROP_MODE_INBETWEEN | Tree::DROP_MODE_ON_ITEM);
		} else {
			if (files.size() > 1) {
				return false;
			}
			tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
		}

		return true;
	}

	if (String(d["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(d["script_list_element"]);
		if (se) {
			String sp = se->get_edited_resource()->get_path();
			if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
				tree->set_drop_mode_flags(Tree::DROP_MODE_ON_ITEM);
				return true;
			}
		}
	}

	return String(d["type"]) == "nodes" && filter.is_empty();
}

void SceneTreeEditor::drop_data_fw(const Point2 &p_point, const Variant &p_data, Control *p_from) {
	if (!can_drop_data_fw(p_point, p_data, p_from)) {
		return;
	}

	TreeItem *item = tree->get_item_at_position(p_point);
	if (!item) {
		return;
	}
	int section = tree->get_drop_section_at_position(p_point);
	if (section < -1) {
		return;
	}

	NodePath np = item->get_metadata(0);
	Node *n = get_node(np);
	if (!n) {
		return;
	}

	Dictionary d = p_data;

	if (String(d["type"]) == "nodes") {
		Array nodes = d["nodes"];
		emit_signal(SNAME("nodes_rearranged"), nodes, np, section);
	}

	if (String(d["type"]) == "files") {
		Vector<String> files = d["files"];

		String ftype = EditorFileSystem::get_singleton()->get_file_type(files[0]);
		if (_is_script_type(ftype)) {
			emit_signal(SNAME("script_dropped"), files[0], np);
		} else {
			emit_signal(SNAME("files_dropped"), files, np, section);
		}
	}

	if (String(d["type"]) == "script_list_element") {
		ScriptEditorBase *se = Object::cast_to<ScriptEditorBase>(d["script_list_element"]);
		if (se) {
			String sp = se->get_edited_resource()->get_path();
			if (_is_script_type(EditorFileSystem::get_singleton()->get_file_type(sp))) {
				emit_signal(SNAME("script_dropped"), sp, np);
			}
		}
	}
}

void SceneTreeEditor::_empty_clicked(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	_rmb_select(p_pos);
}

void SceneTreeEditor::_rmb_select(const Vector2 &p_pos, MouseButton p_button) {
	if (p_button != MouseButton::RIGHT) {
		return;
	}
	emit_signal(SNAME("rmb_pressed"), tree->get_screen_position() + p_pos);
}

void SceneTreeEditor::update_warning() {
	_warning_changed(nullptr);
}

void SceneTreeEditor::_warning_changed(Node *p_for_node) {
	//should use a timer
	update_timer->start();
}

void SceneTreeEditor::set_auto_expand_selected(bool p_auto, bool p_update_settings) {
	if (p_update_settings) {
		EditorSettings::get_singleton()->set("docks/scene_tree/auto_expand_to_selected", p_auto);
	}
	auto_expand_selected = p_auto;
}

void SceneTreeEditor::set_connect_to_script_mode(bool p_enable) {
	connect_to_script_mode = p_enable;
	update_tree();
}

void SceneTreeEditor::set_connecting_signal(bool p_enable) {
	connecting_signal = p_enable;
	update_tree();
}

void SceneTreeEditor::_bind_methods() {
	ClassDB::bind_method(D_METHOD("_update_tree"), &SceneTreeEditor::_update_tree, DEFVAL(false)); // Still used by UndoRedo.
	ClassDB::bind_method("_rename_node", &SceneTreeEditor::_rename_node);
	ClassDB::bind_method("_test_update_tree", &SceneTreeEditor::_test_update_tree);

	ClassDB::bind_method(D_METHOD("_get_drag_data_fw"), &SceneTreeEditor::get_drag_data_fw);
	ClassDB::bind_method(D_METHOD("_can_drop_data_fw"), &SceneTreeEditor::can_drop_data_fw);
	ClassDB::bind_method(D_METHOD("_drop_data_fw"), &SceneTreeEditor::drop_data_fw);

	ClassDB::bind_method(D_METHOD("update_tree"), &SceneTreeEditor::update_tree);

	ADD_SIGNAL(MethodInfo("node_selected"));
	ADD_SIGNAL(MethodInfo("node_renamed"));
	ADD_SIGNAL(MethodInfo("node_prerename"));
	ADD_SIGNAL(MethodInfo("node_changed"));
	ADD_SIGNAL(MethodInfo("nodes_dragged"));
	ADD_SIGNAL(MethodInfo("nodes_rearranged", PropertyInfo(Variant::ARRAY, "paths"), PropertyInfo(Variant::NODE_PATH, "to_path"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("files_dropped", PropertyInfo(Variant::PACKED_STRING_ARRAY, "files"), PropertyInfo(Variant::NODE_PATH, "to_path"), PropertyInfo(Variant::INT, "type")));
	ADD_SIGNAL(MethodInfo("script_dropped", PropertyInfo(Variant::STRING, "file"), PropertyInfo(Variant::NODE_PATH, "to_path")));
	ADD_SIGNAL(MethodInfo("rmb_pressed", PropertyInfo(Variant::VECTOR2, "position")));

	ADD_SIGNAL(MethodInfo("open"));
	ADD_SIGNAL(MethodInfo("open_script"));
}

SceneTreeEditor::SceneTreeEditor(bool p_label, bool p_can_rename, bool p_can_open_instance) {
	selected = nullptr;

	can_rename = p_can_rename;
	can_open_instance = p_can_open_instance;
	editor_selection = nullptr;

	if (p_label) {
		Label *label = memnew(Label);
		label->set_theme_type_variation("HeaderSmall");
		label->set_position(Point2(10, 0));
		label->set_text(TTR("Scene Tree (Nodes):"));

		add_child(label);
	}

	tree = memnew(Tree);
	tree->set_anchor(SIDE_RIGHT, ANCHOR_END);
	tree->set_anchor(SIDE_BOTTOM, ANCHOR_END);
	tree->set_begin(Point2(0, p_label ? 18 : 0));
	tree->set_end(Point2(0, 0));
	tree->set_allow_reselect(true);
	tree->add_theme_constant_override("button_margin", 0);

	add_child(tree);

	tree->set_drag_forwarding(this);
	if (p_can_rename) {
		tree->set_allow_rmb_select(true);
		tree->connect("item_mouse_selected", callable_mp(this, &SceneTreeEditor::_rmb_select));
		tree->connect("empty_clicked", callable_mp(this, &SceneTreeEditor::_empty_clicked));
	}

	tree->connect("cell_selected", callable_mp(this, &SceneTreeEditor::_selected_changed));
	tree->connect("item_edited", callable_mp(this, &SceneTreeEditor::_renamed));
	tree->connect("multi_selected", callable_mp(this, &SceneTreeEditor::_cell_multi_selected));
	tree->connect("button_clicked", callable_mp(this, &SceneTreeEditor::_cell_button_pressed));
	tree->connect("nothing_selected", callable_mp(this, &SceneTreeEditor::_deselect_items));

	error = memnew(AcceptDialog);
	add_child(error);

	warning = memnew(AcceptDialog);
	add_child(warning);
	warning->set_title(TTR("Node Configuration Warning!"));

	last_hash = 0;
	blocked = 0;

	update_timer = memnew(Timer);
	update_timer->connect("timeout", callable_mp(this, &SceneTreeEditor::_update_tree).bind(false));
	update_timer->set_one_shot(true);
	update_timer->set_wait_time(0.5);
	add_child(update_timer);

	script_types = memnew(List<StringName>);
	ClassDB::get_inheriters_from_class("Script", script_types);
}

SceneTreeEditor::~SceneTreeEditor() {
	memdelete(script_types);
}

/******** DIALOG *********/

void SceneTreeDialog::popup_scenetree_dialog() {
	popup_centered_clamped(Size2(350, 700) * EDSCALE);
}

void SceneTreeDialog::_update_theme() {
	filter->set_right_icon(tree->get_theme_icon(SNAME("Search"), SNAME("EditorIcons")));
}

void SceneTreeDialog::_notification(int p_what) {
	switch (p_what) {
		case NOTIFICATION_VISIBILITY_CHANGED: {
			if (is_visible()) {
				tree->update_tree();
			}
		} break;

		case NOTIFICATION_ENTER_TREE: {
			connect("confirmed", callable_mp(this, &SceneTreeDialog::_select));
			_update_theme();
		} break;

		case NOTIFICATION_THEME_CHANGED: {
			_update_theme();
		} break;

		case NOTIFICATION_EXIT_TREE: {
			disconnect("confirmed", callable_mp(this, &SceneTreeDialog::_select));
		} break;
	}
}

void SceneTreeDialog::_cancel() {
	hide();
}

void SceneTreeDialog::_select() {
	if (tree->get_selected()) {
		// The signal may cause another dialog to be displayed, so be sure to hide this one first.
		hide();
		emit_signal(SNAME("selected"), tree->get_selected()->get_path());
	}
}

void SceneTreeDialog::_selected_changed() {
	get_ok_button()->set_disabled(!tree->get_selected());
}

void SceneTreeDialog::_filter_changed(const String &p_filter) {
	tree->set_filter(p_filter);
}

void SceneTreeDialog::_bind_methods() {
	ClassDB::bind_method("_cancel", &SceneTreeDialog::_cancel);

	ADD_SIGNAL(MethodInfo("selected", PropertyInfo(Variant::NODE_PATH, "path")));
}

SceneTreeDialog::SceneTreeDialog() {
	set_title(TTR("Select a Node"));
	VBoxContainer *vbc = memnew(VBoxContainer);
	add_child(vbc);

	filter = memnew(LineEdit);
	filter->set_h_size_flags(Control::SIZE_EXPAND_FILL);
	filter->set_placeholder(TTR("Filter Nodes"));
	filter->set_clear_button_enabled(true);
	filter->add_theme_constant_override("minimum_character_width", 0);
	filter->connect("text_changed", callable_mp(this, &SceneTreeDialog::_filter_changed));
	vbc->add_child(filter);

	tree = memnew(SceneTreeEditor(false, false, true));
	tree->set_v_size_flags(Control::SIZE_EXPAND_FILL);
	tree->get_scene_tree()->connect("item_activated", callable_mp(this, &SceneTreeDialog::_select));
	vbc->add_child(tree);

	// Disable the OK button when no node is selected.
	get_ok_button()->set_disabled(!tree->get_selected());
	tree->connect("node_selected", callable_mp(this, &SceneTreeDialog::_selected_changed));
}

SceneTreeDialog::~SceneTreeDialog() {
}
