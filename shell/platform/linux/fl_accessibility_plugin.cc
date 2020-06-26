// Copyright 2013 The Flutter Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "flutter/shell/platform/linux/fl_accessibility_plugin.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_basic_message_channel.h"
#include "flutter/shell/platform/linux/public/flutter_linux/fl_standard_message_codec.h"

struct _FlAccessibilityPlugin {
  GObject parent_instance;

  FlBasicMessageChannel* channel;

  // Semantics nodes keyed by ID
  GHashTable* semantics_nodes_by_id;
};

G_DEFINE_TYPE(FlAccessibilityPlugin, fl_accessibility_plugin, G_TYPE_OBJECT)

// Handles announce acessibility events from Flutter.
static FlValue* handle_announce(FlAccessibilityPlugin* self, FlValue* data) {
  FlValue* message_value = fl_value_lookup_string(data, "message");
  if (message_value == nullptr ||
      fl_value_get_type(message_value) != FL_VALUE_TYPE_STRING) {
    g_warning("Expected message string");
    return nullptr;
  }
  const gchar* message = fl_value_get_string(message_value);

  g_printerr("ANNOUNCE '%s'\n", message);

  return nullptr;
}

// Handles tap acessibility events from Flutter.
static FlValue* handle_tap(FlAccessibilityPlugin* self, int64_t node_id) {
  g_printerr("TAP '%" G_GINT64_FORMAT "'\n", node_id);

  return nullptr;
}

// Handles long press acessibility events from Flutter.
static FlValue* handle_long_press(FlAccessibilityPlugin* self,
                                  int64_t node_id) {
  g_printerr("LONG-PRESS '%" G_GINT64_FORMAT "'\n", node_id);

  return nullptr;
}

// Handles tooltip acessibility events from Flutter.
static FlValue* handle_tooltip(FlAccessibilityPlugin* self, FlValue* data) {
  FlValue* message_value = fl_value_lookup_string(data, "message");
  if (message_value == nullptr ||
      fl_value_get_type(message_value) != FL_VALUE_TYPE_STRING) {
    g_warning("Expected message string");
    return nullptr;
  }
  const gchar* message = fl_value_get_string(message_value);

  g_printerr("TOOLTIP '%s'\n", message);

  return nullptr;
}

// Handles acessibility events from Flutter.
static FlValue* handle_message(FlAccessibilityPlugin* self, FlValue* message) {
  if (fl_value_get_type(message) != FL_VALUE_TYPE_MAP) {
    g_warning("Ignoring unknown flutter/accessibility message type");
    return nullptr;
  }

  FlValue* type_value = fl_value_lookup_string(message, "type");
  if (type_value == nullptr ||
      fl_value_get_type(type_value) != FL_VALUE_TYPE_STRING) {
    g_warning(
        "Ignoring unknown flutter/accessibility message with unknown type");
    return nullptr;
  }
  const gchar* type = fl_value_get_string(type_value);
  FlValue* data = fl_value_lookup_string(message, "data");
  FlValue* node_id_value = fl_value_lookup_string(message, "nodeId");
  int64_t node_id = -1;
  if (node_id_value != nullptr &&
      fl_value_get_type(node_id_value) == FL_VALUE_TYPE_INT)
    node_id = fl_value_get_int(node_id_value);

  if (strcmp(type, "announce") == 0) {
    return handle_announce(self, data);
  } else if (strcmp(type, "tap") == 0) {
    return handle_tap(self, node_id);
  } else if (strcmp(type, "longPress") == 0) {
    return handle_long_press(self, node_id);
  } else if (strcmp(type, "tooltip") == 0) {
    return handle_tooltip(self, data);
  } else {
    g_debug("Got unknown accessibility message: %s", type);
    return nullptr;
  }
}

// Called when a message is received on this channel
static void message_cb(FlBasicMessageChannel* channel,
                       FlValue* message,
                       FlBasicMessageChannelResponseHandle* response_handle,
                       gpointer user_data) {
  FlAccessibilityPlugin* self = FL_ACCESSIBILITY_PLUGIN(user_data);

  g_autoptr(FlValue) response = handle_message(self, message);

  g_autoptr(GError) error = nullptr;
  if (!fl_basic_message_channel_respond(channel, response_handle, response,
                                        &error))
    g_warning("Failed to send message response: %s", error->message);
}

static void fl_accessibility_plugin_dispose(GObject* object) {
  FlAccessibilityPlugin* self = FL_ACCESSIBILITY_PLUGIN(object);

  g_clear_object(&self->channel);

  G_OBJECT_CLASS(fl_accessibility_plugin_parent_class)->dispose(object);
}

static void fl_accessibility_plugin_class_init(
    FlAccessibilityPluginClass* klass) {
  G_OBJECT_CLASS(klass)->dispose = fl_accessibility_plugin_dispose;
}

static void fl_accessibility_plugin_init(FlAccessibilityPlugin* self) {
  self->semantics_nodes_by_id =
      g_hash_table_new_full(g_int_hash, g_int_equal, nullptr, nullptr);
}

FlAccessibilityPlugin* fl_accessibility_plugin_new(
    FlBinaryMessenger* messenger) {
  g_return_val_if_fail(FL_IS_BINARY_MESSENGER(messenger), nullptr);

  FlAccessibilityPlugin* self = FL_ACCESSIBILITY_PLUGIN(
      g_object_new(fl_accessibility_plugin_get_type(), nullptr));

  g_autoptr(FlStandardMessageCodec) codec = fl_standard_message_codec_new();
  self->channel = fl_basic_message_channel_new(
      messenger, "flutter/accessibility", FL_MESSAGE_CODEC(codec));
  fl_basic_message_channel_set_message_handler(self->channel, message_cb, self,
                                               nullptr);

  return self;
}

static gboolean has_flag(FlutterSemanticsFlag* flags,
                         FlutterSemanticsFlag flag) {
  if ((*flags & flag) == 0)
    return FALSE;

  *flags = static_cast<FlutterSemanticsFlag>(*flags ^ flag);
  return TRUE;
}

static gboolean has_action(FlutterSemanticsAction* actions,
                           FlutterSemanticsAction action) {
  if ((*actions & action) == 0)
    return FALSE;

  *actions = static_cast<FlutterSemanticsAction>(*actions ^ action);
  return TRUE;
}

void fl_accessibility_plugin_handle_update_semantics_node(
    FlAccessibilityPlugin* plugin,
    const FlutterSemanticsNode* node) {
  if (node->id == kFlutterSemanticsCustomActionIdBatchEnd) {
    g_printerr("Semantics Nodes End\n");
    return;
  }

  g_printerr("Semantics Node\n");
  g_printerr("  id: %d\n", node->id);
  if (node->flags != 0) {
    g_printerr("  flags:");
    FlutterSemanticsFlag flags = node->flags;
    if (has_flag(&flags, kFlutterSemanticsFlagHasCheckedState))
      g_printerr(" HasCheckedState");
    if (has_flag(&flags, kFlutterSemanticsFlagIsChecked))
      g_printerr(" IsChecked");
    if (has_flag(&flags, kFlutterSemanticsFlagIsSelected))
      g_printerr(" IsSelected");
    if (has_flag(&flags, kFlutterSemanticsFlagIsButton))
      g_printerr(" IsButton");
    if (has_flag(&flags, kFlutterSemanticsFlagIsTextField))
      g_printerr(" IsTextField");
    if (has_flag(&flags, kFlutterSemanticsFlagIsFocused))
      g_printerr(" IsFocused");
    if (has_flag(&flags, kFlutterSemanticsFlagHasEnabledState))
      g_printerr(" HasEnabledState");
    if (has_flag(&flags, kFlutterSemanticsFlagIsEnabled))
      g_printerr(" IsEnabled");
    if (has_flag(&flags, kFlutterSemanticsFlagIsInMutuallyExclusiveGroup))
      g_printerr(" IsInMutuallyExclusiveGroup");
    if (has_flag(&flags, kFlutterSemanticsFlagIsHeader))
      g_printerr(" IsHeader");
    if (has_flag(&flags, kFlutterSemanticsFlagIsObscured))
      g_printerr(" IsObscured");
    if (has_flag(&flags, kFlutterSemanticsFlagScopesRoute))
      g_printerr(" ScopesRoute");
    if (has_flag(&flags, kFlutterSemanticsFlagNamesRoute))
      g_printerr(" NamesRoute");
    if (has_flag(&flags, kFlutterSemanticsFlagIsHidden))
      g_printerr(" IsHidden");
    if (has_flag(&flags, kFlutterSemanticsFlagIsImage))
      g_printerr(" IsImage");
    if (has_flag(&flags, kFlutterSemanticsFlagIsLiveRegion))
      g_printerr(" IsLiveRegion");
    if (has_flag(&flags, kFlutterSemanticsFlagHasToggledState))
      g_printerr(" HasToggledState");
    if (has_flag(&flags, kFlutterSemanticsFlagIsToggled))
      g_printerr(" IsToggled");
    if (has_flag(&flags, kFlutterSemanticsFlagHasImplicitScrolling))
      g_printerr(" HasImplicitScrolling");
    if (has_flag(&flags, kFlutterSemanticsFlagIsReadOnly))
      g_printerr(" IsReadOnly");
    if (has_flag(&flags, kFlutterSemanticsFlagIsFocusable))
      g_printerr(" IsFocusable");
    if (has_flag(&flags, kFlutterSemanticsFlagIsLink))
      g_printerr(" IsLink");
    if (flags != 0)
      g_printerr(" 0x%x", flags);
    g_printerr("\n");
  }
  if (node->actions != 0) {
    g_printerr("  actions:");
    FlutterSemanticsAction actions = node->actions;
    if (has_action(&actions, kFlutterSemanticsActionTap))
      g_printerr(" Tap");
    if (has_action(&actions, kFlutterSemanticsActionLongPress))
      g_printerr(" LongPress");
    if (has_action(&actions, kFlutterSemanticsActionScrollLeft))
      g_printerr(" ScrollLeft");
    if (has_action(&actions, kFlutterSemanticsActionScrollRight))
      g_printerr(" ScrollRight");
    if (has_action(&actions, kFlutterSemanticsActionScrollUp))
      g_printerr(" ScrollUp");
    if (has_action(&actions, kFlutterSemanticsActionScrollDown))
      g_printerr(" ScrollDown");
    if (has_action(&actions, kFlutterSemanticsActionIncrease))
      g_printerr(" Increase");
    if (has_action(&actions, kFlutterSemanticsActionDecrease))
      g_printerr(" Decrease");
    if (has_action(&actions, kFlutterSemanticsActionShowOnScreen))
      g_printerr(" ShowOnScreen");
    if (has_action(&actions,
                   kFlutterSemanticsActionMoveCursorForwardByCharacter))
      g_printerr(" MoveCursorForwardByCharacter");
    if (has_action(&actions,
                   kFlutterSemanticsActionMoveCursorBackwardByCharacter))
      g_printerr(" MoveCursorBackwardByCharacter");
    if (has_action(&actions, kFlutterSemanticsActionSetSelection))
      g_printerr(" SetSelection");
    if (has_action(&actions, kFlutterSemanticsActionCopy))
      g_printerr(" Copy");
    if (has_action(&actions, kFlutterSemanticsActionCut))
      g_printerr(" Cut");
    if (has_action(&actions, kFlutterSemanticsActionPaste))
      g_printerr(" Paste");
    if (has_action(&actions, kFlutterSemanticsActionDidGainAccessibilityFocus))
      g_printerr(" DidGainAccessibilityFocus");
    if (has_action(&actions, kFlutterSemanticsActionDidLoseAccessibilityFocus))
      g_printerr(" DidLoseAccessibilityFocus");
    if (has_action(&actions, kFlutterSemanticsActionCustomAction))
      g_printerr(" CustomAction");
    if (has_action(&actions, kFlutterSemanticsActionDismiss))
      g_printerr(" Dismiss");
    if (has_action(&actions, kFlutterSemanticsActionMoveCursorForwardByWord))
      g_printerr(" MoveCursorForwardByWord");
    if (has_action(&actions, kFlutterSemanticsActionMoveCursorBackwardByWord))
      g_printerr(" MoveCursorBackwardByWord");
    if (actions != 0)
      g_printerr(" 0x%x", actions);
    g_printerr("\n");
  }
  if (node->text_selection_base != -1)
    g_printerr("  text_selection_base: %d\n", node->text_selection_base);
  if (node->text_selection_extent != -1)
    g_printerr("  text_selection_extent: %d\n", node->text_selection_extent);
  if (node->scroll_child_count != 0)
    g_printerr("  scroll_child_count: %d\n", node->scroll_child_count);
  if (node->scroll_index != 0)
    g_printerr("  scroll_index: %d\n", node->scroll_index);
  if (!isnan(node->scroll_position))
    g_printerr("  scroll_position: %g\n", node->scroll_position);
  if (!isnan(node->scroll_extent_max))
    g_printerr("  scroll_extent_max: %g\n", node->scroll_extent_max);
  if (!isnan(node->scroll_extent_min))
    g_printerr("  scroll_extent_min: %g\n", node->scroll_extent_min);
  if (node->elevation != 0)
    g_printerr("  elevation: %g\n", node->elevation);
  if (node->thickness != 0)
    g_printerr("  thickness: %g\n", node->thickness);
  if (node->label[0] != '\0')
    g_printerr("  label: %s\n", node->label);
  if (node->hint[0] != '\0')
    g_printerr("  hint: %s\n", node->hint);
  if (node->value[0] != '\0')
    g_printerr("  value: %s\n", node->value);
  if (node->increased_value[0] != '\0')
    g_printerr("  increased_value: %s\n", node->increased_value);
  if (node->decreased_value[0] != '\0')
    g_printerr("  decreased_value: %s\n", node->decreased_value);
  if (node->text_direction != kFlutterTextDirectionUnknown) {
    g_printerr("  text_direction: ");
    switch (node->text_direction) {
      case kFlutterTextDirectionRTL:
        g_printerr("RTL");
        break;
      case kFlutterTextDirectionLTR:
        g_printerr("LTR");
        break;
      default:
        g_printerr("%d\n", node->text_direction);
        break;
    }
    g_printerr("\n");
  }
  g_printerr("  rect: %g %g %g %g (lrtb)\n", node->rect.left, node->rect.right,
             node->rect.top, node->rect.bottom);
  if (node->transform.transX != 0 || node->transform.transY != 0 ||
      node->transform.scaleX != 1 || node->transform.scaleY != 1 ||
      node->transform.skewX != 0 || node->transform.skewY != 0 ||
      node->transform.pers0 != 0 || node->transform.pers1 != 0 ||
      node->transform.pers2 != 1) {
    g_printerr("  transform:");
    if (node->transform.transX != 0 || node->transform.transY != 0)
      g_printerr(" translate(%g, %g)", node->transform.transX,
                 node->transform.transY);
    if (node->transform.scaleX != 1 || node->transform.scaleY != 1)
      g_printerr(" scale(%g, %g)", node->transform.scaleX,
                 node->transform.scaleY);
    if (node->transform.skewX != 0 || node->transform.skewY != 0)
      g_printerr(" skew(%g, %g)", node->transform.skewX, node->transform.skewY);
    if (node->transform.pers0 != 0 || node->transform.pers1 != 0 ||
        node->transform.pers2 != 1)
      g_printerr(" perspective(%g, %g, %g)", node->transform.pers0,
                 node->transform.pers1, node->transform.pers2);
    g_printerr("\n");
  }
  if (node->child_count > 0) {
    g_printerr("  children_in_traversal_order:");
    for (size_t i = 0; i < node->child_count; i++)
      g_printerr(" %d", node->children_in_traversal_order[i]);
    g_printerr("\n");
    g_printerr("  children_in_hit_test_order:");
    for (size_t i = 0; i < node->child_count; i++)
      g_printerr(" %d", node->children_in_hit_test_order[i]);
    g_printerr("\n");
  }
  if (node->custom_accessibility_actions_count > 0) {
    g_printerr("  custom_accessibility_actions:");
    for (size_t i = 0; i < node->custom_accessibility_actions_count; i++)
      g_printerr(" %d", node->custom_accessibility_actions[i]);
    g_printerr("\n");
  }
  if (node->platform_view_id != -1)
    g_printerr("  platform_view_id: %" G_GINT64_FORMAT "\n",
               node->platform_view_id);
}