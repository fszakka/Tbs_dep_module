<?php

/**
 * Menu callback; List all department types available.
 */
function department_overview_types() {
  foreach (department_types() as $type => $info) {
    $type_url_str = str_replace('_', '-', $type);
    $label = t('View @type', array('@type' => $info->name));
    $items[] = l($label, 'admin/structure/department/manage/' . $type_url_str);
  }

  return theme('item_list', array('items' => $items));
}

/**
 * Menu callback; Artwork information page.
 *
 * @param object $artwork_type
 */
function department_information($department_type) {
  return $department_type->name . ': ' . $department_type->description;
}
