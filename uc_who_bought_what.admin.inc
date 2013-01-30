<?php

/* * *****************************************************************************
 * Hook Functions (Ubercart)
 * **************************************************************************** */

function uc_who_bought_what_form($form_state, $node = NULL) {
  global $user;

  $listquery = "SELECT DISTINCT({uc_products}.nid) AS nid, {node}.title AS title, {uc_product_classes}.name AS type, {node}.status as status
    FROM {uc_products}
    INNER JOIN {node} ON {uc_products}.nid = {node}.nid
    LEFT JOIN {uc_product_classes} ON {uc_product_classes}.pcid = {node}.type
		WHERE status = status";

  if (user_access('view all who bought what'))
    $listquery .= ''; // do nothing. see all
  else {
    if (user_access('view uc_who_bought_what'))
      $listquery .= " AND {node}.uid = $user->uid";  //limit to my stuff
    drupal_set_message("Only displaying reports for items posted by $user->name.");
  };

  $listquery .= " ORDER BY {uc_product_classes}.name, {node}.title";

  $products = db_query($listquery);

  $product_list[0] = t('Select an item...');

  while ($product = db_fetch_array($products)) {
    if ($product['type'] != NULL)
      $type = $product['type'];
    else
      $type = "Product";
    $product_list[$type][$product['nid']] = $product['title'];
  }

  $form['filter'] = array(
    '#type' => 'fieldset',
    '#title' => t('Filter'),
  );

  $form['filter']['product_select'] = array(
    '#type' => 'select',
    '#title' => t('Choose a product'),
    '#options' => $product_list,
    '#default_value' => $node ? $node->nid : NULL,
    '#description' => t('Choose a product.'),
  );

  $form['filter']['submit'] = array('#type' => 'submit', '#value' => t('Submit'));

  if ($node) {
    # $form['reports']['#value'] = uc_who_bought_what_report($node);

    $form['reports'] = array(
      '#type' => 'fieldset',
      '#title' => t('Report'),
      '#value' => uc_who_bought_what_report($node),
    );
  }

  return $form;
}

// function uc_who_bought_what_form

/* * *****************************************************************************
 * Callback Functions, Forms, and Tables
 * **************************************************************************** */

function uc_who_bought_what_form_validate($form_id, $form_values) {
  if ($form_values['product_select'] == '0') {
    form_set_error('', t('You must select a product.'));
  }
}

// function uc_who_bought_what_validate

function uc_who_bought_what_form_submit($form, $form_values) {
  drupal_goto("admin/store/reports/uc_who_bought_what/{$form_values['values']['product_select']}");
}

// function uc_who_bought_what_form_submit

/**
 * Display the purchaser list report
 */
function uc_who_bought_what_report($node) {
  $product_id = $node->nid;
  $page = (!is_null($_GET['page'])) ? intval($_GET['page']) : 0;
  $page_size = (!is_null($_GET['nopage'])) ? UC_REPORTS_MAX_RECORDS : variable_get('uc_reports_table_size', 1000);

  $csv_rows = array();

  $output = '';
  $uc_order_products = db_prefix_tables('{uc_order_products}');
  $uc_orders = db_prefix_tables('{uc_orders}');
  $uc_node_checkout_order_products = db_prefix_tables('{uc_node_checkout_order_products}');
  $uc_countries = db_prefix_tables('{uc_countries}');
  $uc_zones = db_prefix_tables('{uc_zones}');

  // if a node was specified, it better be one you can look at!
  if (uc_product_is_product($node)) {
    if (!node_access('view', $node)) {
      drupal_set_message('You are not authorized to view who bought that item.  Please contact your site administrator', 'warning');
      return drupal_access_denied();
    }
    $checkout = FALSE;

    //What about node checkout?  Better test first.

    if (module_exists('uc_node_checkout')) {
      $uc_node_checkout_order_products = db_prefix_tables('{uc_node_checkout_order_products}');
      $uc_node_checkout_types = db_prefix_tables('{uc_node_checkout_types}');
      $sql = "SELECT node_type, product_nid from $uc_node_checkout_types WHERE product_nid = $product_id";
      $checknode = db_fetch_array(db_query($sql));
      if ($checknode) {
        drupal_set_message("Also Displaying linked nodes (beta).");
        $checkout = TRUE;
      }
    } //end if node checkout module


    $tql = "SELECT COUNT($uc_order_products.qty) AS ordcount, SUM($uc_order_products.qty) AS grandtotal FROM $uc_order_products" .
      " JOIN $uc_orders ON $uc_order_products.order_id = $uc_orders.order_id " .
      " AND $uc_order_products.nid  =  $product_id  WHERE order_status <> 'in_checkout' AND order_status <> 'canceled' ";
    $tquery = db_query($tql);
    $tresult = db_fetch_object($tquery);

    $output .= '<h3>' . l($node->title, 'node/' . $node->nid) . " ($tresult->grandtotal sold in $tresult->ordcount orders)</h3>";

    $header = array(//thanks to blackice78 for this code fix
      array('data' => t('Qty'), 'field' => 'qty', 'sql_field' => "$uc_order_products.qty"),
      array('data' => t('Username'), 'field' => 'name', 'sql_field' => "$uc_orders.uid"), // added by L0rne
      array('data' => t('LName'), 'field' => 'lname', 'sql_field' => "$uc_orders.billing_last_name"),
      array('data' => t('FName'), 'field' => 'fname', 'sql_field' => "$uc_orders.billing_first_name"),
      array('data' => t('Email'), 'field' => 'email', 'sql_field' => "$uc_orders.primary_email"),
      array('data' => t('Phone'), 'field' => 'phone', 'sql_field' => "$uc_orders.billing_phone"),
      array('data' => t('Street1'), 'field' => 'street1', 'sql_field' => "$uc_orders.billing_street1"),
      array('data' => t('Street2'), 'field' => 'street2', 'sql_field' => "$uc_orders.billing_street2"),
      array('data' => t('City'), 'field' => 'city', 'sql_field' => "$uc_orders.billing_city"),
      array('data' => t('Zone'), 'field' => 'zone', 'sql_field' => "uccbz.zone_name"),
      array('data' => t('Postal code'), 'field' => 'postal_code', 'sql_field' => "$uc_orders.billing_postal_code"),
      array('data' => t('Country'), 'field' => 'country', 'sql_field' => "uccb.country_name"),
      array('data' => t('Price'), 'field' => 'price', 'sql_field' => "$uc_order_products.price"),
      array('data' => t('Coupon')),
      array('data' => t('Discount')),
      array('data' => t('Paid')),
      array('data' => t('Total'), 'field' => 'total', 'sql_field' => "($uc_order_products.price * $uc_order_products.qty)"),
      array('data' => t('Date'), 'field' => 'date', 'sql_field' => "$uc_orders.created"),
      array('data' => t('HowPay'), 'field' => 'howpay', 'sql_field' => "$uc_orders.payment_method"),
      array('data' => t('Status'), 'field' => 'status', 'sql_field' => "$uc_orders.order_status"),
      array('data' => t('ID'), 'field' => 'id', 'sql_field' => "$uc_orders.order_id"),
      array('data' => t('Model'), 'field' => 'model', 'sql_field' => "$uc_order_products.model"),
    );

    $sqsel = "  "; //now build the sql query
    foreach ($header as $hval) {
      if ($hval['sql_field']) {
        $sqsel .= $hval['sql_field'] . " AS " . $hval['field'];
        $sqsel .= ", ";
      }
    }
    $sqsel .= "$uc_orders.uid" . " AS uid , $uc_orders.*,  uccb.country_name AS bill_country, uccs.country_name AS ship_country, ";
    $sqsel .= "uccbz.zone_code AS bill_zone, uccsz.zone_code AS ship_zone, ";

    if ($checkout)
      $sqsel .= "$uc_node_checkout_order_products.nid" . " AS checkoutnode ,"; // if it exists

    $sqsel .= "$uc_order_products.data AS orderdata "; // this is to collect the raw attribute data and also not to have a comma at the end
    $sql = "SELECT  $sqsel FROM $uc_orders " .
      " JOIN $uc_order_products" .
      " ON $uc_order_products.order_id = $uc_orders.order_id " .
      " AND $uc_order_products.nid  =  $product_id " .
      " LEFT JOIN $uc_countries uccb ON $uc_orders.billing_country = uccb.country_id " .
      " LEFT JOIN $uc_countries uccs ON $uc_orders.delivery_country = uccs.country_id " .
      " LEFT JOIN $uc_zones uccbz ON $uc_orders.billing_country = uccbz.zone_country_id AND $uc_orders.billing_zone = uccbz.zone_id " .
      " LEFT JOIN $uc_zones uccsz ON $uc_orders.delivery_country = uccsz.zone_country_id AND $uc_orders.delivery_zone = uccsz.zone_id ";

    if ($checkout)
      $sql .= " LEFT JOIN $uc_node_checkout_order_products"; // if it exists
    if ($checkout)
      $sql .= " ON $uc_node_checkout_order_products.order_product_id = $uc_order_products.order_product_id";

    $sql .= " WHERE order_status <> 'in_checkout' AND order_status <> 'canceled' ";


    $customers = pager_query($sql . tablesort_sql($header), $page_size, 0, $sql_count);
    $email_addresses = array();  // we like to collect the addresses.
    //What about node checkout?  Better test first.
    if ($checkout)
      $header[] = array('data' => $checknode['node_type']);  // we did the search earlier


//What about the attributes?  Not everyone uses attributes.  Better test first.

    if (module_exists('uc_attribute')) {
      $uc_attributes = db_prefix_tables('{uc_attributes}');
      $uc_product_attributes = db_prefix_tables('{uc_product_attributes}');

      //Collect and unserialize the attributes.  First, the names
      $attsql = "SELECT att.aid, att.ordering, att.name, IF(LENGTH(uca.label)>0,uca.label, att.name) AS label, uca.nid FROM $uc_attributes att
      JOIN $uc_product_attributes uca ON uca.aid=att.aid AND nid = %d Order By ordering, name";
      $attresults = (db_query($attsql, $product_id));

      while ($attrow = db_fetch_array($attresults)) {
        $header[] = array('data' => t($attrow['label']));
        $attrlist[] = array('data' => t($attrow['label']));
      }
    } //end if attribute module exists
    // Display dicounted price
    $duplicate_value = array(5, 6, 7, 8, 9, 10);
    $csv_rows = array();  //hold the plain values.

    $temp_csv = array();
    foreach ($header as $key => $new_csv_val) {
      if (!in_array($key, $duplicate_value)) {
        $temp_csv[] = strip_tags($new_csv_val['data']);
      }
    }
    $csv_xtra = array('delivery_first_name', 'delivery_last_name', 'delivery_phone', 'delivery_company', 'delivery_street1', 'delivery_street2', 'delivery_city', 'ship_zone', 'delivery_postal_code', 'ship_country', 'billing_first_name', 'billing_last_name', 'billing_phone', 'billing_company', 'billing_street1', 'billing_street2', 'billing_city', 'bill_zone', 'billing_postal_code', 'bill_country');
    foreach ($csv_xtra as $newhead) {
      $temp_csv[] = $newhead;
    }
    $csv_rows[] = $temp_csv;

    while ($customer = db_fetch_array($customers)) {
      uc_who_bought_what_report__row($node->nid, $customer, $email_addresses, $rows, $csv_rows, $csv_xtra, $duplicate_value);
    }

    if (empty($rows)) {
      $rows[] = array(array('data' => t('No customers found for this item'), 'colspan' => count($header)));
    }
    $csv_data = uc_reports_store_csv('who_bought_' . convert_to_filename($node->title), $csv_rows);

    $output .= theme('table', $header, $rows, array('width' => '100%', 'class' => 'uc-sales-table'));
    $output .= theme_pager(NULL, $page_size);

    $output .= '<div class="uc-reports-links">' . l(t('Export ' . (sizeof($csv_rows) - 1) . ' rows to csv file'), 'admin/store/reports/getcsv/' . $csv_data['report'] . '/' . $csv_data['user']) . '&nbsp;&nbsp;&nbsp;' . (!is_null($_GET['nopage']) ? l(t('Show paged records'), "admin/store/reports/uc_who_bought_what/{$product_id}") : l(t('Show all records'), "admin/store/reports/uc_who_bought_what/{$product_id}", array('query' => "nopage=1"))) . '</div>';
    $output .= '<br />';
    $output .= '<div class="email-addresses"><h4>Email Addresses</h4><label for="email-addresses-textarea" value="Email addresses:" /><textarea id="email-addresses-textarea" rows="5" cols="100">' . implode(', ', $email_addresses) . '</textarea></div>';
  }

  return $output;
}

// function uc_who_bought_what_report

/**
 * Converts a string to a valid UNIX filename.
 * @param $string The filename to be converted
 * @return $string The filename converted
 */
function convert_to_filename($string) {
  // Replace spaces with underscores and makes the string lowercase
  $string = str_replace(" ", "_", $string);

  $string = str_replace("..", ".", $string);
  $string = strtolower($string);

  // Match any character that is not in our whitelist
  preg_match_all("/[^0-9^a-z^_^.]/", $string, $matches);

  // Loop through the matches with foreach
  foreach ($matches[0] as $value) {
    $string = str_replace($value, "", $string);
  }

  return $string;
}

function uc_who_bought_what_report__row($nid, $customer, &$email_addresses, &$rows, &$csv_rows, $csv_xtra, $duplicate_value) {
  $temparray = array();
  $temparray[] = $customer['qty'];
  $temparray[] = l(check_plain(user_load($customer['uid'])->name), 'user/' . $customer['uid']); // added by L0rne
  $temparray[] = l(check_plain($customer['lname']), 'user/' . $customer['uid']);
  $temparray[] = l(check_plain($customer['fname']), 'user/' . $customer['uid']);
  $temparray[] = l($customer['email'], 'mailto:' . $customer['email']);
  $temparray[] = check_plain($customer['phone']);
  $temparray[] = check_plain($customer['billing_street1']);
  $temparray[] = check_plain($customer['billing_street2']);
  $temparray[] = check_plain($customer['billing_city']);
  $temparray[] = check_plain($customer['bill_zone']);
  $temparray[] = check_plain($customer['billing_postal_code']);
  $temparray[] = check_plain($customer['bill_country']);
  $temparray[] = number_format($customer['price'], 2);

  // Display discounted price
  $order = uc_order_load($customer['order_id']);
  foreach ($order->products as $i => $product) {
    if ($nid != $product->nid) {
      unset($order->products[$i]);
    }
  }

  $paid = $customer['price'];

  if (!empty($order->data) && isset($order->data['coupon'])) {
    if ($coupon = uc_coupon_validate($order->data['coupon'], $order->order_id)) {
      $coupon = $coupon->value;
      $paid -= $coupon;
    }
  }

  if ($discounts = get_discounts_for_order($order, $errors, $messages)) {
    $dis = 0;
    foreach ($discounts as $discount) {
      $dis += $discount->amount;
    }
    $discount = number_format($dis, 2);
    $paid -= $discount;
  }

  $temparray[] = $coupon;
  $temparray[] = $discount;
  $temparray[] = number_format($paid, 2);
  $temparray[] = number_format($customer['total'], 2);
  $temparray[] = strftime('%Y.%m.%d', $customer['date']);
  $temparray[] = $customer['howpay'];
  $temparray[] = l($customer['status'], 'admin/store/orders/' . $customer['id']);
  $temparray[] = l("#" . $customer['id'], 'admin/store/orders/' . $customer['id']);
  $temparray[] = check_plain($customer['model']);

  $email_addresses[] = $customer['email'];  //add to the list
  //now to deal with the serialized data, including node checkout and attributes

  $mydata = unserialize($customer['orderdata']);

  if ($checkout && isset($mydata['node_checkout_nid'])) {
    $nodeval = $mydata['node_checkout_nid'];
    $mychk = node_load($nodeval);
    $temparray[] = l($mychk->title, 'node/' . $nodeval);
  };


  if (!is_null($attrlist)) {
    foreach ($attrlist as $myattrib) {
      $thisdatum = $mydata['attributes'][$myattrib['data']];
      if (is_array($thisdatum) == 1) {   //Since UC 6.x-2.0-rc6, attributes are stored as arrays just in case there is more than one selected.
        $temparray[] = check_plain(implode(' | ', $thisdatum));
      } else {
        $temparray[] = check_plain($thisdatum);
      }
    }
  }

  $rows[] = $temparray;

  foreach ($csv_xtra as $newhead) {
    $temparray[] = check_plain($customer[$newhead]);
  }

  $temp_csv = array();

  foreach ($duplicate_value as $key) {
    unset($temparray[$key]);
  }

  foreach ($temparray as $new_csv_val) {
    $temp_csv[] = strip_tags($new_csv_val); // do not need to check_plain because we did that above
  }

  $csv_rows[] = $temp_csv;
}
