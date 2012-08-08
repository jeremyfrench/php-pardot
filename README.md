php-pardot
==========

A php connector to the pardot API. 

The Pardot API is documented here
http://developer.pardot.com/kb/api-version-3/introduction-table-of-contents


## Usage

There are currently two objects in this library. One for handling communication with the API and one for allowing CRU(minus the D) options on Prospects.

A basic workflow would be something like this.

```php
// Create a client object.
$client =  new pardotAPIClient('user','password','key');
// Load the prospect john doe.
$propspect = pardotProspect::load('john.doe@example.com', $pardot_api);
// Output the current country.
var_dump($propspect->getCountry());
// Set the country to be united states.
$propspect->setCountry('United States');
// Save the prospect back to Pardot.
$prospect->save();

```


## Usage with Drupal. 

The pardotAPIClient allows for pluggable transport and logging methods. So it is possible to use the methods applicable to third party frameworks. 

Currently there is a drupal wrapper which uses drupal_http_request and watchdog.

This can be invoked in the following way.

```php
module_load_include('inc', 'mymodule', 'libs/drupalWrapper');
$pardot_api = pardot_wrapper_api_factory('user','password','key');
```

# Custom Fields for prospects

Pardot allows configurable 'custom' fields for prospects. To implement these in code you can override the pardotPropsect object.

```php
class icesPardotProspect extends pardotProspect {
  function setFavouriteFlavour($network_type) {
    $this->setValue('favourite_flavour , $network_type);
  }

  function getFavouriteFlavour() {
    return $this->getValue('favourite_flavour');
  }
}
```

to create a prospect in this case you would do
```php
$client =  new pardotAPIClient('user','password','key');
// Load the prospect john doe, using the Ices subclass.
$propspect = icesPardotProspect::load('john.doe@example.com', $pardot_api);
```