<tbody>
  <tr>
    <td colspan="2">
      <div style="font-weight: bold">cwmp</div>
      <table style="width:100%">
        <tbody>
          <tr>
            <td>
              <div style="font-weight: bold; font-size: 14px">section</div>
            </td>
            <td>
              <div style="font-weight: bold; font-size: 14px">description</div>
            </td>
            <td>
              <div style="font-weight: bold; font-size: 14px">multi</div>
            </td>
            <td>
              <div style="font-weight: bold; font-size: 14px">options</div>
            </td>
          </tr>
          <tr>
            <td class="td_row_even">
              <div class="td_row_even">acs</div>
            </td>
            <td class="td_row_even">
              <div class="td_row_even">Configure the ACS parameters, used by icwmp</div>
            </td>
            <td class="td_row_even">
              <div class="td_row_even">false</div>
            </td>
            <td class="td_row_even">
              <table style="width:100%">
                <tbody>
                  <tr>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">name</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">type</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">required</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">default</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">description</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">url</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">yes</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">URL of ACS server</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">userid</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Username for ACS server connection</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">passwd</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Password for ACS server connection</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">periodic_inform_enable</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">1</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If set to **1**, the CPE must periodically open session with ACS by sending Inform message to the ACS.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">periodic_inform_interval</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">uinteger</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">86400</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">The duration in seconds of the interval for which the CPE must attempt to connect with the ACS and call the Inform method.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">periodic_inform_time</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">uinteger</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">An absolute time reference to determine when the CPE will initiate the periodic Inform method calls.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">dhcp_discovery</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">if set to **enable**, the CPE will get the url of ACS from DHCP server Option 43.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">compression</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">if set to **1**, the CPE must use the HTTP Compression when communicating with the ACS.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">retry_min_wait_interval</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">uinteger</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">5</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">The minimum wait interval for session retry (in seconds)</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">retry_interval_multiplier</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">uinteger</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">2000</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">The retry interval multiplier for session retry session as described in the standard.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">ssl_capath</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">The path of ssl certificates for TR-069 sessions.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">insecure_enable</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">if set to **1**, the CPE skips validation of the ACS certificates.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">http_disable_100continue</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">if set to **1**, disables the http 100 continue behaviour.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">dhcp_url</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">the **url** of ACS server received from the DHCP server via Option 43. This parameter is automatically updated by daemon, When **'dhcp_discovery'** option is enabled.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">ip_version</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">IP version of ConnectionRequestURL</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">get_rpc_methods</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">1</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Enable/Disable optional GetRPCMethods to ACS</div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </td>
          </tr>
          <tr>
            <td class="td_row_odd">
              <div class="td_row_odd">cpe</div>
            </td>
            <td class="td_row_odd">
              <div class="td_row_odd">CWMP client configuration</div>
            </td>
            <td class="td_row_odd">
              <div class="td_row_odd">false</div>
            </td>
            <td class="td_row_odd">
              <table style="width:100%">
                <tbody>
                  <tr>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">name</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">type</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">required</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">default</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">description</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">enable</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">1</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">if set to **1**, the cwmp client will be enabled. </div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">manufacturer</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">manufacturer_oui</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">product_class</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">serial_number</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">software_version</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">model_name</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">description</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Overwrite DeviceId parameter</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">default_lan_interface</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">lan</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Configure the default lan interface of the device.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">default_wan_interface</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">yes</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">wan</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Configure the default wan interface that will be used for IPv4/IPv6 connection.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">interface</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Use this option to define l3 device ifname directly, if this option is used default_wan_interface shall be ignored</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">use_curl_ifname</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">false</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If this option is set, then cwmp shall use l3 device ifname as CURLOPT_INTERFACE</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">log_to_console</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">If set to **1**, the log messages will be shown in the console/stdout.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">log_to_file</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If set to **1**, the log messages will be saved in the log file.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">log_severity</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Specifies the log type to use, by default **'INFO'**. The possible types are **'EMERG', 'ALERT', 'CRITIC' ,'ERROR', 'WARNING', 'NOTICE', 'INFO' and 'DEBUG'**.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">log_file_name</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">/var/log/icwmpd.log</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Specifies the path of the log file, by default **'/var/log/icwmpd.log'**.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">log_max_size</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">uinteger</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">102400</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Size of the log file. The default value is **102400**.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">userid</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">The username of the device used in a connection request from ACS to CPE.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">passwd</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">The password of the device when sending a connection request from ACS to CPE.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">port</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">uinteger</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">7547</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">The port used for connection request.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">path</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">/</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">CPE Connection request URI path</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">provisioning_code</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Specifies the primary service provider and other provisioning information, which may be used by the ACS to determine service provider-specific customization and provisioning parameters.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">amd_version</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">uinteger</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">5</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Configure the amendment version to use.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">instance_mode</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">InstanceNumber</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Configure the instance mode to use. Supported instance modes are : &lt;B&gt;InstanceNumber&lt;/B&gt; and &lt;B&gt;InstanceNumber&lt;/B&gt;.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">session_timeout</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">uinteger</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">60</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Represents the number of seconds that should be used by the ACS as the amount of time to wait before timing out a CWMP session due to the CPE not responding.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">notification</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If set to **1**, it enables the notification feature.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">exec_download</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">If set to **1**, Specifies if Download method is executed.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">log_to_syslog</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If set to **1**, the cwmp log will be appended to busybox syslog.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">periodic_notify_enable</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">1</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">If set to **1**, icwmp will be able to detect parameter value change at any time.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">periodic_notify_interval</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">integer</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">10</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Interval in sec to check for value change notifications</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">custom_notify_json</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Define absolute path of the JSON containing parameters on which notification get enabled as per the definition. See readme for examples.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">incoming_rule</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">This configure firewall rules. Allowed values &lt;IP_Only/Port_Only/IP_Port&gt;. IP_Only means only acs ip as source ip used for firewall input rule, Port_Only means only destination port will be used and IP_Port or empty value meaning both ip and port will be used for firewall input rule.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">force_ipv4</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">If set to 1, it forces the connectivity over v4 IP address.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">fw_upgrade_keep_settings</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">boolean</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">1</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">If set to **1**, icwmp will keep the uci setting while doing firmware upgrade using Download RPC with '1 Firmware Upgrade Image'.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">disable_datatype_check</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">0</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">If set to **1**, icwmp will skip datatype validation on SPV operations.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">ssl_cert_path</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">string</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd"></div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">Full path of SSL certificate in pem format, icwmp will send this certificate to ACS server for authentication.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">ssl_key_path</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">string</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even"></div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">Full path of the pem file that has stored the key</div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </td>
          </tr>
          <tr>
            <td class="td_row_even">
              <div class="td_row_even">lwn</div>
            </td>
            <td class="td_row_even">
              <div class="td_row_even">Lightweight notification configuration</div>
            </td>
            <td class="td_row_even">
              <div class="td_row_even">false</div>
            </td>
            <td class="td_row_even">
              <table style="width:100%">
                <tbody>
                  <tr>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">name</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">type</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">required</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">default</div>
                    </td>
                    <td>
                      <div style="font-weight: bold; font-size: 14px">description</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">enable</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">boolean</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">0</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">if set to **1**, the Lightweight Notifications will be enabled. </div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_odd">
                      <div class="td_row_odd">hostname</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">host</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">no</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">ACS url</div>
                    </td>
                    <td class="td_row_odd">
                      <div class="td_row_odd">The hostname or address to be used when sending the UDP Lightweight Notifications.</div>
                    </td>
                  </tr>
                  <tr>
                    <td class="td_row_even">
                      <div class="td_row_even">port</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">port</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">no</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">7547</div>
                    </td>
                    <td class="td_row_even">
                      <div class="td_row_even">The port number to be used when sending UDP Lightweight Notifications.</div>
                    </td>
                  </tr>
                </tbody>
              </table>
            </td>
          </tr>
        </tbody>
      </table>
    </td>
  </tr>
</tbody>