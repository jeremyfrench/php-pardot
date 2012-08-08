<?php
/**
 * @file
 * Include file with base pardot objects and helper methods.
 * 
 * @author  Jeremy French <jeremy@jeremfrench.co.uk>
 * 
 * @section LICENSE
 * 
 * Copyright 2012 Alfresco Software Limited.
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


/**
 * Support Functions
 * 
 * Some methods can be 'injected' into the pardot API Client this allows 
 * different php frameworks to use their own communication or logging handlers
 * however in the basic case functional handers can be proveded. Any implementations
 * will have to follow the same function signitures. 
 * 
 */ 

/**
 * A basic http hander for communicating to the pardot api.
 * file_get_contents will have to be able to call http for this to work.
 * 
 * @param string $url the full URL to call.
 * @param string $http_header additional HTTP headers to add, this should be in 
 * a text block. each header on a new line and value seperated from header by 
 * a colon.  
 * @param string $http_method http method to use GET or POST
 */
function pardot_basic_http_agent($url, $http_header = 'Content-type: application/x-www-form-urlencoded', $http_method = 'GET') {
  $opts = array('http' =>
      array(
          'method'  => $http_method,
          'header'  => $http_header,
      )
  );
  
  $context  = stream_context_create($opts);
  return file_get_contents($url, false, $context);
}

/**
 * The pardotAPIClient class handles communication with the Pardot API.
 * @author jfrench
 *
 */
class pardotAPIClient {
  const API_VERSION_STRING = '/version/3';
  const BASE_URL = 'https://pi.pardot.com/api/';
  
  // user key and api key shoudn't change once object is constructed.
  protected  $user_key;
  static $api_key;
  protected $http_agent;
  protected $logging_agent;
  /**
   * Construct a partdot API Client object.
   * @param string $username the username used to access pardot.
   * @param string $password the password user to access pardot.
   * @param string $user_key the user key which is available on a pardot users profile page.
   * @param string $http_agent the name of a method to handle http communication for the pardot API Client.
   * @param string $logging_agent the name of a method to perform logging actions for the pardot API Client.
   * @throws Exception
   */
  function __construct($username, $password, $user_key, $http_agent = 'pardot_basic_http_agent', $logging_agent = NULL) {
    $this->username = $username;
    $this->password = $password;
    $this->user_key = $user_key;
    $this->http_agent = $http_agent;
    $this->logging_agent = $logging_agent;
    // If no API key has been created in this session then add a new one. 
    /* TODO: this may have to be cached per user in cases where multple users 
      are using the client in one php request */
    if(!isset(self::$api_key)) {
      $this->_refreshAPIkey();
    }
  }

  /**
   * Wrapper for the logging agent. 
   */
  protected function _log($message) {
    if ($this->logging_agent != NULL) {
      call_user_func($this->logging_agent, $message);
    }
  }

  /**
   * Provide a wrapper to the http method allowing for timed out sessions parsing
   * the XML
   */
  protected function _pardot_request($path,$entity,$params = array()) {
      $url =  self::BASE_URL . $entity . self::API_VERSION_STRING . $path;
      
      $trys = 0;
      
      while ($trys < 2) {
        $params['user_key'] = $this->user_key;
        $params['api_key'] = self::$api_key;
        
        $query_data = http_build_query( $params , '', '&' );
        
        if (strstr('?',$url)) {
          $full_url = $url . $query_data;
        }
        else {
          $full_url = $url . '?' . $query_data;
        }
      $result = call_user_func($this->http_agent, $full_url, 'Content-type: application/x-www-form-urlencoded', 'GET');
      
      $xml_result = simplexml_load_string( $result);
      if($xml_result != FALSE)
      $root_attributes = $xml_result->attributes();
      if($root_attributes['stat'] == 'fail') {
        // Get error code and message 
        $error_element = $xml_result->err;
        $error_attributes = $error_element->attributes();
        $code = $error_attributes['code'];
        $message = (string) $error_element;
        $this->_log($message);
        if ($code == 1) {
          $this->_refreshAPIkey();
        }
        else {
          // Only retry on invalid api key error. Other errors handled upstream.
          return $xml_result;
        }
      }
      else {
        return $xml_result;
      }
      $trys ++;
    }
    $this->_log('no valid response from pardot giving up');
    return null;
  }
  
  /**
   * Perform a login action to the pardot API to get an API key which will then
   * be used in subsiquent requests.
   * 
   * This gist was used orignially but has been extensivly reworked
   * https://gist.github.com/2821580
   * 
   * @throws Exception if a login is not possible.
   */
  protected function _refreshAPIkey() {
    $url = self::BASE_URL . 'login' . self::API_VERSION_STRING . '?';
    $data = array();
    $data['email'] = $this->username;
    $data['password'] = $this->password;
    $data['user_key'] = $this->user_key;
    $query_data = http_build_query( $data , '', '&' );
    $result = call_user_func($this->http_agent, $url . $query_data, 'Content-type: application/x-www-form-urlencoded', 'POST');
  
    $xml_response = simplexml_load_string( $result);
    if(!isset($xml_response->api_key)) {
      $this->_log('Unable to log into pardot, Logging Error');
      throw new Exception('Unable to log into pardot');
    }
    //TODO: check for error and throw
    $this->_log('Logged into Pardot');
    self::$api_key = (string) $xml_response->api_key;
  }

  /**
   * Return a standard pardot prospect object given an email address.
   * @param string $email_address the email address of the prospect.
   * @return NULL|pardotProspect
   */
  function getProspectByEmail($email_address) {
    $prospect_xml = $this->getProspectXML($email_address);
    if ($prospect_xml == NULL) {
      return null;
    }
    $prospect = new pardotProspect($prospect_xml ,$this);
    return $prospect;
  }
  
  /**
   * Return the raw prospect XML
   * @param string $email_address the email address of the prospect.
   * @return null|SimpleXml
   * @see http://developer.pardot.com/kb/api-version-3/using-prospects#xml-response-format
   */
  function getProspectXML($email_address) {
    $url = '/do/read/email/' . $email_address;
    $result = $this->_pardot_request($url,'prospect');
    $root_attributes = $result->attributes();
    if($root_attributes['stat'] == 'fail') {
      return null;
    }
    $prospect_xml = $result->prospect;
    return $prospect_xml;
  }
  
  /**
   * Create a prospect given an email address
   * @param string $email_address the email address of a prospect that is not yet in pardot.
   * @param array $values key are attribute names and values are the values.
   * @see http://developer.pardot.com/kb/api-version-3/using-prospects#creating-prospects
   */
  function createProspect($email_address, $values) {
    $url = '/do/create/email/' . $email_address;
    $result = $this->_pardot_request($url,'prospect',$values);
    $root_attributes = $result->attributes();
    if($root_attributes['stat'] == 'fail') {
      $this->_log('could not create account with email '. $email_address);
      return null;
    }
    return $result->prospect;
  }
  /**
   * Update a known prospect with new values.
   * @param string $email_address the email address of the prospect you wish to 
   * update.
   * @param array $new_values the new values for the properties key is property name
   * @see http://developer.pardot.com/kb/api-version-3/using-prospects#updating-field-values
   */
  function updateProspect($email_address,$new_values = array()) {
    $url = '/do/update/email/' . $email_address;
    $result = $this->_pardot_request($url,'prospect',$new_values);

    $root_attributes = $result->attributes();
     
    if($root_attributes['stat'] == 'fail') {
      return null;
    }
    return $result->prospect;
    
  }
  
  }

/**
 * a standard pardotProspect object. Extend this class to provide functionality 
 * for custom fields. 
 * @author jfrench
 *
 */
class pardotProspect {
  private $prospect_xml;
  private $api_client;
  private $overrides;
  
  /**
   * Create a new pardotProspect object.
   * @param simpleXml|string $prospect_info if a prospect is loaded from pardot
   * use the simpleXML as this parameter to perform actions on the prospect. 
   * If this is a new prospect use a string with the email address. 
   * @param pardotAPIClient $api_client a loaded API client which will be used
   * to communicate with the pardot API.
   */
  function __construct($prospect_info, $api_client) {
    if(is_string($prospect_info)) {
      $this->new = TRUE;
      $this->prospect_xml = new stdClass();
      $this->overrides = array(
            'email' => $prospect_info,
         );
    }
    else {
      $this->new = FALSE;
      $this->prospect_xml = $prospect_info;
      $this->overrides = array();
    }
    $this->api_client = $api_client;
  }
  
  // For each property do a getter and setter. Store set properties so that a 
  // save will know what properties to add. 
  // TODO: more properties.
  
  /**
   * Get the ID of the pardotProspect
   * 
   * @return Ambigous <string, NULL>
   */
  function getID() {
    return $this->getValue('id');
  }
  
  /**
   * Set the first name of the prospect
   * @param string $name
   */
  function setFirstName($name) {
    $this->setValue('first_name', $name);
  }
  
  /**
   * Get the first name of the prospect.
   * @return Ambigous <string, NULL>
   */
  function getFirstName() {
    return $this->getValue('first_name');
  }
  
  /**
   * Set the last name of the prospect
   * @param string $name
   */
  function setLastName($name) {
    $this->setValue('last_name', $name);
  }
  
  /**
   * Get the last name of the prospect
   * @return Ambigous <string, NULL>
   */
  function getLastName() {
    return $this->getValue('last_name');
  }
  
  // country
  function setCountry($country) {
    $this->setValue('country', $country);
  }
  
  function getCountry() {
    return $this->getValue('country');
  }
  
  /**
   * Get the state (geographical) of the prospect
   * @param string $state
   */
  function setState($state) {
    $this->setValue('state', $state);
  }
  
  function getState() {
    return $this->getValue('state');
  }
  
  // Last Active Date
  function getLastActive() {
    $date_string = $this->getValue('last_activity_at');
    return strtotime($date_string);
  }
  
  
  // Source
  function setSource($source) {
    $this->setValue('source', $source);
  }
  
  
  function getSource() {
    return $this->getValue('source');
  }
  
  function getCampaign() {
    return $this->getValue('campaign');
  }
  
  function setCampaignId($campaign) {
    $this->setValue('campaign_id',$campaign);
  }

  /**
   * Add a subscription to a list with a given id.
   * @param unknown_type $list_id
   */
  function subscribeToList($list_id) {
    $search = array();
    if (isset($this->prospect_xml->xpath)) {
      $search = $this->prospect_xml->xpath('//list/id[text()="'.$list_id.'"]');
    }
    if(sizeof($search) == 0) {
      $this->overrides['list_' . $list_id] = 1;
    }
  }
  
  // email
  function getEmail() {
    return $this->getValue('email');
  }
  
  /**
   * Set a value in the object. This will be placed in the overries array if 
   * the value is not equal to the exsisting value and will be updated when a 
   * save mehod is called on this object.
   * 
   * Protected to keep the interface clean. To set a value implent a set method.
   * If it is a custom value override the pardotProspect method and create a set
   * method in there. 
   * 
   * @param string $key
   * @param string $value
   */
  protected function setValue($key,$value) {
    // If the key has not be overwritten and the being set is the same as the
    // stored value then don't update.
    if (!isset($this->overrides[$key]) && isset($this->prospect_xml->{$key})) {
      $old_value = (string) $this->prospect_xml->{$key};
      if ($old_value == $value) {
        return;
      }
    }
    $this->overrides[$key] = $value;
  }
  
  /**
   * Get a value from the XML, if the value has been changed get it from the 
   * overrides array.
   * @param string $key
   * @return string|NULL
   */
  protected function getValue($key) {
    if(isset($this->overrides[$key])) {
      return $this->overrides[$key];
    }
    elseif (isset($this->prospect_xml->{$key})) {
      return  (string) $this->prospect_xml->{$key};
    }
    else {
      return null;
    }
  }
  
  /**
   * Has the object been modified since being loaded. Fields which are set to 
   * the same value do not show as modified. 
   * @return integer the size of the change, 0 meens no changes
   */
  function modified() {
    return sizeof($this->overrides);
  }
  
  /**
   * Save or create this pardotProspect to the pardotAPI.
   * New propsects will have the create method called, exsisting prospects will 
   * have the update method called.
   * 
   * If no changes have been made to the prospect update will not be called. 
   */
  function save() {
    if ($this->new) {
      $email = $this->overrides['email'];
      unset($this->overrides['email']);
      $this->prospect_xml = $this->api_client->createProspect($email,$this->overrides);
      $this->new = FALSE;
    }
    else {
      if ($this->modified() != 0) {
        // TODO: error checking.
        $this->prospect_xml = $this->api_client->updateProspect($this->getEmail(), $this->overrides);
        $this->overrides = array();
      }
    }
  }
  
  /** 
   * Load a pardot prospect object wiht a given email address. This can be used
   * with subclasses of the pardotProspect.
   * 
   * @param string $email_address
   * @param pardotAPIClient $api_client
   */
  static function load($email_address,$api_client) {
    $prospect_xml = $api_client->getProspectXml($email_address);
    if ($prospect_xml == NULL) {
      return false;
    }
    $called_class = get_called_class();
    return new $called_class($prospect_xml,$api_client);
  }
  
}
