<?php

/**
 * Menu callback; Show list of department types we can add.
 */
function artwork_add_page() {
  $item = menu_get_item();
  $links = system_admin_menu_block($item);

  foreach ($links as $link) {
    $items[] = l($link['title'], $link['href'], $item['localized_options'])
      . ': ' . filter_xss_admin($link['description']);
  }

  return theme('item_list', array('items' => $items));
}

/**
 * Present an department submission form.
 */
function department_add($type) {
  global $user;
 
  $types = department_types();
  $type = isset($type) ? str_replace('-', '_', $type) : NULL;
  
  
  if (empty($types[$type])) {
    return MENU_NOT_FOUND;
  }

  $department = entity_get_controller('department')->create($type);
 
  drupal_set_title(t('Create @name', array('@name' => $types[$type]->name)), PASS_THROUGH);
  return drupal_get_form($type . '_department_form', $department);
}

/**
 * Menu callback; presents the department editing form, or redirects to delete confirmation.
 *
 * @param $department
 *   The artwork object to edit.
 */
function department_page_edit($department) {
  $types = department_types();
  drupal_set_title(t('<em>Edit @type</em> @title', array('@type' => $types[$department->dep_type]->name, '@title' => $department->dep_name)), PASS_THROUGH);

  return drupal_get_form($artwork->dep_type . '_department_form', $department);
}


/**
 * Form builder; Displays the artwork add/edit form.
 *
 * @param $form
 * @param $form_state
 * @param $artwork
 *   The artwork object to edit, which may be brand new.
 */
function department_form($form, &$form_state, $department) {

  // Set the id and identify this as an artwork edit form.
  $form['#id'] = 'department-form';

  // Save the artwork for later, in case we need it.
  $form['#department'] = $department;
  $form_state['department'] = $department;

  // Common fields. We don't have many.
  $form['title'] = array(
    '#type' => 'textfield',
    '#title' => t('Title'),
    '#default_value' => $department->dep_name,
    '#weight' => -5,
    '#required' => TRUE,
  );

  $form['revision'] = array(
    '#access' => user_access('administer departments'),
    '#type' => 'checkbox',
    '#title' => t('Create new revision'),
    '#default_value' => 0,
  );

  // Add the buttons.
  $form['buttons'] = array();
  $form['buttons']['#weight'] = 100;
  $form['buttons']['submit'] = array(
    '#type' => 'submit',
    '#value' => t('Save'),
    '#weight' => 5,
    '#submit' => array('department_form_submit'),
  );
  if (!empty($artwork->aid)) {
    $form['buttons']['delete'] = array(
      '#access' => user_access('delete departments'),
      '#type' => 'submit',
      '#value' => t('Delete'),
      '#weight' => 15,
      '#submit' => array('department_form_delete_submit'),
    );
  }

  $form['#validate'][] = 'department_form_validate';

  field_attach_form('department', $department, $form, $form_state);

  return $form;
}

function department_form_validate($form, &$form_state) {
  $department = $form_state['department'];

  // Field validation.
  field_attach_form_validate('department', $department, $form, $form_state);
}


function department_form_submit($form, &$form_state) {
  global $user;

  $department = &$form_state['department'];

  // Set the artwork's uid if it's being created at this time.
  if (empty($department->dep_id)) {
    $department->dep_id = $user->uid;
  }

  $department->dep_name = $form_state['values']['title'];
  $department->dep_vid = $form_state['values']['revision'];

  // Notify field widgets.
  field_attach_submit('department', $department, $form, $form_state);

  // Save the artwork.
  artwork_save($department);

  // Notify the user.
  drupal_set_message(t('Artwork saved.'));

  $form_state['redirect'] = 'department/' . $department->dep_id;
}

function department_form_delete_submit($form, &$form_state) {
  $destination = array();
  if (isset($_GET['destination'])) {
    $destination = drupal_get_destination();
    unset($_GET['destination']);
  }
  $department = $form['#department'];
  $form_state['redirect'] = array('department/' . $department->dep_id . '/delete', array('query' => $destination));
}

/**
 * Displays an department.
 *
 * @param $artwork
 *   The artwork object to display.
 * @param $view_mode
 *   The view mode we want to display.
 */
function department_page_view($department, $view_mode = 'full') {
  // Remove previously built content, if exists.
  $department->content = array();

  if ($view_mode == 'teaser') {
    $department->content['title'] = array(
      '#markup' => filter_xss($department->title),
      '#weight' => -5,
    );
  }

  // Build fields content.
  field_attach_prepare_view('department', array($department->dep_id => $department), $view_mode);
  entity_prepare_view('department', array($department->dep_id => $department));
  $department->content += field_attach_view('department', $department, $view_mode);
  
  return $department->content;
}

/**
 * Form bulder; Asks for confirmation of artwork deletion.
 */
function department_delete_confirm($form, &$form_state, $department) {
  $form['#department'] = $department;
  // Always provide entity id in the same form key as in the entity edit form.
  $form['dep_id'] = array('#type' => 'value', '#value' => $department->dep_id);
  return confirm_form($form,
    t('Are you sure you want to delete %title?', array('%title' => $department->dep_name)),
    'department/' . $department->dep_id,
    t('This action cannot be undone.'),
    t('Delete'),
    t('Cancel')
  );
}
/**
 * Executes artwork deletion.
 */
function artwork_delete_confirm_submit($form, &$form_state) {
  if ($form_state['values']['confirm']) {
    $department = department_load($form_state['values']['dep_id']);
    department_delete($form_state['values']['dep_id']);
    watchdog('department', '@type: deleted %title.', array('@type' => $department->dep_type, '%title' => $department->title));

    $types = department_types();
    drupal_set_message(t('@type %title has been deleted.', array('@type' => $types[$department->dep_type]->name, '%title' => $department->title)));
  }

  $form_state['redirect'] = '<front>';
}

function department_page_list_recent() {
  $content = array();

  $query = new EntityFieldQuery();
  $query
    ->entityCondition('entity_type', 'department')
    ->propertyOrderBy('created', 'DESC')
    ->fieldCondition('field_department', 'value', 'Accra', 'CONTAINS', 0)
    ->range(0, 5);
  $result = $query->execute();

  $departments = department_load_multiple(array_keys($result['department']));
  foreach ($deparments as $department) {
    $content[$artwork->dep_id] = artwork_page_view($department, 'teaser');
  }

  return $content;
}
