## MQTT Provisioning Contract

### mesh/provisioning/edgeid
Comma-separated edge IDs
Example: 01,02,03

### mesh/provisioning/ca
PEM encoded certificate string

### mesh/provisioning/hmac
Format: <HEX_KEY><MMDDHHMM>
Example: 7AB0...F35004011030

Timestamp format:
- MM = month (01-12)
- DD = day (01-31)
- HH = hour (00-23)
- MM = minute (00-59)

Example:
04011030 = April 1st, 10:30 UTC

### edge/provisioning/hmac
Format: <HEX_KEY>
Example: 7AB0...F350

### Encoding
- All payload is UTF-8 encoded text
- No trailing newline

### MQTT settings
- QoS: 1
- Retain: false