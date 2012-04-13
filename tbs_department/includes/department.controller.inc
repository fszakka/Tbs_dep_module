<?php

/**
 * Controller for loading, creating, and saving departments
 *
 * The default loader, which we extend, handles load() already. We only
 * need to add saving and creating.
 */
class DepartmentController extends DrupalDefaultEntityController {
    
    public function save($department){
        $transaction = db_transaction();
        
        try {
            global $user;
            
            // Determin if we will be inserting a new department
            $department->is_new = empty($department->dep_id);
            
            // Set the timestamp fields.
            if(empty($department->dep_created)) {
                $department->dep_created = REQUEST_TIME;
            }
            $department->dep_changed = REQUEST_TIME;

            $department->revision_timestamp = REQUEST_TIME;
            $update_department = TRUE;
            
            // Give modules the opportunity to prepare field data for saving.
            field_attach_presave('department',$department);
            
            //When saving a new department revision, unset any existing $department->dep_vid
            //to ensure a new revision will actually be created and store the old
            //revision ID in a separate property for department hook implementations.
            if(!$department->is_new && !empty($department->revision) && $department->dep_vid){
                $department->old_dep_vid = $department->dep_vid;
                unset($department->dep_vid);
            }
            
            // if this is a new department
            if($department->is_new){
                //save the new department
                drupal_write_record('department',$department);
                
                // Save the initial revision
                $this->saveRevision($department, $user->uid);
                
                $op = 'insert';
                                    
            }
            else {
                // Save the updated department
                drupal_write_record('department',$department, 'dep_id');
                
                // If a new department revision was  requested, save a new record for that;
                //otherwise, update the department revision record that matches the value
                // of $department->dep_id
                if(!empty($department->revision)){
                    $this->saveRevision($department, $user->uid);
                }
                else {
                    $this->saveRevision($department, $user->uid, TRUE);
                    $update_department = FALSE;
                }
                
                $op = 'update';
            }
            // If the revision ID is new or updated, save it to the department
            if($update_department) {
                db_update('tbs_department')
                    ->fields(array('dep_vid' => $department->dep_vid))
                    ->condition('dep_id' $department->dep_id)
                    ->execute();
            }
            
            // Save fields.
            $function = 'field_attach_' . $op;
            $function('department',$department);
            
            module_invoke_all('entity_' . $op, $department, 'department');
            
            //Clear internal properties
            unset($department->is_new);
            
            // Ignore slave server temporarily to give time for the saved order to be
            // propagated to the slave.
            db_ignore_slave();
            
            return $department;

        }
        catch(Exception $e) {
            $transaction->rollback();
            watchdog_exception('department', $e, NULL, WATCHDOG_ERROR);
            return FALSE;
        }
    }
    
  /**
   * Saves an department revision with the uid of the current user.
   *
   * @param $department
   *   The fully loaded artwork object.
   * @param $uid
   *   The user's uid for the current revision.
   * @param $update
   *   TRUE or FALSE indicating whether or not the existing revision should be
   *     updated instead of a new one created.
   */
  function saveRevision($department, $uid, $update = FALSE){
     // Hold on to the artwork's original creator_uid but swap in the revision's
    // creator_uid for the momentary write.
    $temp_uid = $department->dep_id;
    $department->dep_uid = $uid;

    // Update the existing revision if specified.
    if ($update) {
      drupal_write_record('tbs_department_revision', $department, 'dep_rev_vid');
    }
    else {
      // Otherwise insert a new revision. This will automatically update $artwork
      // to include the vid.
      drupal_write_record('tbs_department_revision', $department);
    }

    // Reset the order's creator_uid to the original value.
    $department->dep_id = $temp_uid;
  }
  
  /**
   * Deletes multiple artworks by ID.
   *
   * @param $aids
   *   An array of artwork IDs to delete.
   * @return
   *   TRUE on success, FALSE otherwise.
   */
  public function delete($aids) {
    if (!empty($aids)) {
      $department = $this->load($aids, array());

      $transaction = db_transaction();

      try {
        db_delete('tbs_department')
          ->condition('dep_id', $aids, 'IN')
          ->execute();

        db_delete('tbs_department_revision')
          ->condition('dep_rev_id', $aids, 'IN')
          ->execute();

        foreach ($department as $department_id => $artwork) {
          field_attach_delete('department', $department);
        }

        // Ignore slave server temporarily to give time for the
        // saved artwork to be propagated to the slave.
        db_ignore_slave();
      }
      catch (Exception $e) {
        $transaction->rollback();
        watchdog_exception('department', $e, NULL, WATCHDOG_ERROR);
        return FALSE;
      }

      module_invoke_all('entity_delete', $department, 'department');

      // Clear the page and block and artwork caches.
      cache_clear_all();
      $this->resetCache();
    }

    return TRUE;
  }
  
  /**
   * Create a default department.
   *
   * @param $type
   *   The machine-readable type of the artwork.
   *
   * @return
   *   An artwork object with all default fields initialized.
   */
  public function create($type = '') {
    return (object) array(
      'dep_id' => '',
      'dep_type' => $type,
      'dep_name' => '',
    );
  }
}