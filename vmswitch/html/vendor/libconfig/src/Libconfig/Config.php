<?php
namespace Radiergummi\Libconfig;

/**
 * General purpose config class
 * 
 * @package libconfig
 * @author Radiergummi <m@9dev.de>
 */
class Config extends \ArrayObject
{

  /**
   * holds the configuration data
   * 
   * (default value: array())
   * 
   * @var array
   * @access private
   */
  private $data = array();

  /**
   * holds the current instance
   * 
   * (default value: null)
   * 
   * @var object
   * @access private
   */
  private $instance = null;

  /**
   * instance function.
   * creates a new instance of the class for chaining
   * 
   * @access public
   * @return object  the current config object
   * 
   */
  public static function instance()
  {
    // create a new instance if we have none
    if (is_null($instance)) $instance = new static();
    
    return static::$instance;
  }


  /**
   * __construct function.
   * Populates the data array with the values injected at runtime
   * 
   * @access public
   * @param mixed $data  The raw input data
   * @return void
   */
  public function __construct($data)
  {
    $this->add($data);
    parent::__construct($this->data);
  }
  

  /**
   * parseJSON function.
   * parses the input given as a json string
   * 
   * @access private
   * @param string $input  a JSON string
   * @return array $data  the parsed data
   */
  private function parseJSON($input)
  {
    // JSON error message strings
    $errorMessages = array(
      'No error has occurred',
      'The maximum stack depth has been exceeded',
      'Invalid or malformed JSON',
      'Control character error, possibly incorrectly encoded',
      'Syntax Error',
      'Malformed UTF-8 characters, possibly incorrectly encoded'
    );

    $data = json_decode($input, true);
    if (($error = json_last_error()) != 0) {
      throw new \RuntimeException('Error while parsing JSON: ' . $errorMessages[$error] . ' ("' . substr($input, 0, 20) . '...")');
    }

    return $data;
  }


  /**
   * add function.
   * merges the config data with another array
   * 
   * @access public
   * @param string|array $input  the data to add
   * @return void
   */
  public function add($input)
  {
    // if we got a string, it can be either JSON, a filename or a folder path.
    if (is_string($input)) {
      
      // if we got a directory, add each file separately again (except . and ..).
      if (is_dir($input)) {
        if (! is_readable($input)) throw new \RuntimeException('Directory "' . $input . '" is not readable.');

				// add each file in a directory to the config
        foreach (array_diff(scandir($input), array('..', '.')) as $file) {
          $this->add(rtrim($input, DIRECTORY_SEPARATOR) . DIRECTORY_SEPARATOR . $file);
        }

        return;
      }
      
      // if we got a filename, decide how to handle it based on the extension, then add its parsed content again
      if (is_file($input)) {
        if (! is_readable($input)) throw new \RuntimeException('File "' . $input . '" is not readable.');

          switch(pathinfo($input, PATHINFO_EXTENSION)) {
            case 'php':
              $content = require($input);
              break;

            case 'json':
              $content = $this->parseJSON(file_get_contents($input));
              break;

            // example INI implementation
            #case 'ini':
            #  $content = $this->parseINI(file_get_contents($input));
            #  break;
          }

        $this->add($content);

        return;
      }

      // string input is generally treated as JSON
      $this->add($this->parseJSON($input));

      return;
    }

    // if we got an array, it has either been injected as one or parsed already. Either way it will be merged now.
    if (is_array($input)) {
      $this->data = array_replace_recursive($this->data, $input);

      return;
    }

    // if we have no match, throw an exception (this happens if neither a string nor an array was given).
    throw new \RuntimeException('Provided data could not be parsed ("' . substr($input, 0, 20) . '...").');
    return;
  }


  /**
   * get function.
   * gets a value from the config array
   * 
   * @access public
   * @param mixed $key (default: null)  the config key in question
   * @param mixed $fallback (default: null)  a fallback value in case the config is empty
   * @return string  the value of $data[$key]
   */
  public function get($key = null, $fallback = null)
  {

    // return the whole config if no key specified
    if (! $key) return $this->data;

    $keys = explode('.', $key);
    $values = $this->data;

    if (count($keys) == 1) {

      return (array_key_exists($keys[0], $values) ? $values[$keys[0]] : $fallback);
    } else {

      // search the array using the dot character to access nested array values
      foreach($keys as $key) {

        // when a key is not found or we didnt get an array to search return a fallback value
        if(! array_key_exists($key, $values)) {

          return $fallback;
        }

        $values =& $values[$key];
      }

      return $values;
    }
  }

  /**
   * has function.
   * checks wether a key is set
   * 
   * @access public
   * @param string $key  the config key in question
   * @return bool  wether the key is set
   */
  public function has($key)
  {
    return (! is_null($this->get($key))) ? true : false;  
  }


  /**
   * set function.
   * sets a value in the config array
   * 
   * @access public
   * @param string $key  the config key in question
   * @param mixed $value  the value to set 
   * @return void
   */
  public function set($key, $value)
  {
    $array =& $this->data;
    $keys = explode('.', $key);

    // traverse the array into the second last key
    while(count($keys) > 1) {
      $key = array_shift($keys);

      // make sure we have an array to set our new key in
      if( ! array_key_exists($key, $array)) {
        $array[$key] = array();
      }
      $array =& $array[$key];
    }
    $array[array_shift($keys)] = $value;
  }


  /**
   * erase function.
   * erases a key from the config
   * 
   * @access public
   * @param string $key  the key to remove
   * @return void
   */
  public function erase($key)
  {
    $array =& $this->data;
    $keys = explode('.', $key);
    // traverse the array into the second last key
    while(count($keys) > 1) {
      $key = array_shift($keys);
      $array =& $array[$key];
    }
    unset($array[array_shift($keys)]);
  }


  /**
  * __tostring function.
  * returns the complete config array as serialized data.
  * 
  * @access public
  * @return array  $data the complete config array as serialized data
  */
  public function __tostring()
  {
    return serialize($this->data);
  }
 
  /**
  * Iterator Interface
  *
  */
  public function getIterator() {
    return new \ArrayIterator($this->data);
  }

  /**
  * Count Interface
  *
  */
 
  /**
   * count function.
   * 
   * @access public
   * @return int
   */
  public function count()
  {
    return count($this->data);
  }
}
