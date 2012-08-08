<?php
/**
 * @file
 * A wrapper to allow the drupal framework to work well with the pardot API.
 * 
 * @author  Jeremy French <jeremy@jeremfrench.co.uk>
 * 
 * @section LICENSE
 * 
 * Copyright 2012 Alfresco Software Limited.
 * 
 * 
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * 
 *     http://www.apache.org/licenses/LICENSE-2.0
 * 
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * 
 */

// Include the main library
include_once 'pardot.inc';

/**
 * Use drupal_http_request as the http agent for the pardot API. 
 * @param string $url the full URL to call.
 * @param string $http_header additional HTTP headers to add, this should be in 
 * a text block. each header on a new line and value seperated from header by 
 * a colon.  
 * @param string $http_method http method to use GET or POST
 */
function pardot_wrapper_http_method($url, $http_header = 'Content-type: application/x-www-form-urlencoded', $http_method = 'GET') {
  $options = array();
  if ($http_header != '') {
    $headers = explode('\n', $http_header);
    $header_option = array();
    foreach($headers as $header) {
      $header_split = explode(':', $header);
      $header_option[$header_split[0]] = trim($header_split[1]);
    }
    $options['headders'] = $header_option;
  }
  $options['method'] = $http_method;
  $options['timeout'] = 60;
  $result = drupal_http_request($url,$options);
  return $result->data;
}

/**
 * Use watchdog for pardot logging events. 
 * @param string $message
 */
function pardot_wrapper_logging_method($message) {
  watchdog('pardot_api',$message);
}

/**
 * Create a pardot API Client which uses the drupal http and logging methods. 
 * @param string $user
 * @param string $password
 * @param string $key
 * @return pardotAPIClient
 */
function pardot_wrapper_api_factory($user,$password,$key) {
  return new pardotAPIClient($user, $password, $key,'pardot_wrapper_http_method','pardot_wrapper_logging_method');
}

