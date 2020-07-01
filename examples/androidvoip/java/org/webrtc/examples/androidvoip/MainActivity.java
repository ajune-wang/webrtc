/*
 *  Copyright (c) 2020 The WebRTC project authors. All Rights Reserved.
 *
 *  Use of this source code is governed by a BSD-style license
 *  that can be found in the LICENSE file in the root of the source
 *  tree. An additional intellectual property rights grant can be found
 *  in the file PATENTS.  All contributing project authors may
 *  be found in the AUTHORS file in the root of the source tree.
 */

package org.webrtc.examples.androidvoip;

import android.Manifest.permission;
import android.app.Activity;
import android.app.AlertDialog;
import android.content.Context;
import android.content.pm.PackageManager;
import android.graphics.Color;
import android.os.Bundle;
import android.view.Gravity;
import android.view.View;
import android.widget.AdapterView;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.RelativeLayout;
import android.widget.ScrollView;
import android.widget.Spinner;
import android.widget.Switch;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;
import androidx.core.app.ActivityCompat;
import androidx.core.content.ContextCompat;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.HashSet;
import java.util.List;
import java.util.Set;
import java.util.stream.Collectors;
import org.webrtc.ContextUtils;

public class MainActivity extends Activity {
  private VoipClient voipClient;
  private boolean isClientInitialized;
  private List<String> supportedEncoders;
  private List<String> supportedDecoders;
  private boolean[] isDecoderSelected;
  private Set<Integer> selectedDecoders;

  private Toast toast;
  private ScrollView scrollView;
  private TextView localIPAddressTextView;
  private EditText localPortNumberEditText;
  private EditText remoteIPAddressEditText;
  private EditText remotePortNumberEditText;
  private Spinner encoderSpinner;
  private Button decoderSelectionButton;
  private TextView decodersTextView;
  private ToggleButton sessionButton;
  private RelativeLayout switchLayout;
  private Switch sendSwitch;
  private Switch playoutSwitch;

  @Override
  protected void onCreate(Bundle savedInstance) {
    ContextUtils.initialize(getApplicationContext());

    super.onCreate(savedInstance);
    setContentView(R.layout.activity_main);

    System.loadLibrary("examples_androidvoip_jni");

    voipClient = new VoipClient();
    isClientInitialized = voipClient.initialize();
    if (!isClientInitialized) {
      showToast("Error initializing");
    }

    supportedEncoders = voipClient.getSupportedEncoders();
    supportedDecoders = voipClient.getSupportedDecoders();
    isDecoderSelected = new boolean[supportedDecoders.size()];
    selectedDecoders = new HashSet<>();

    toast = Toast.makeText(this, "", Toast.LENGTH_SHORT);

    scrollView = (ScrollView) findViewById(R.id.scroll_view);
    localIPAddressTextView = (TextView) findViewById(R.id.local_ip_address_text_view);
    localPortNumberEditText = (EditText) findViewById(R.id.local_port_number_edit_text);
    remoteIPAddressEditText = (EditText) findViewById(R.id.remote_ip_address_edit_text);
    remotePortNumberEditText = (EditText) findViewById(R.id.remote_port_number_edit_text);
    encoderSpinner = (Spinner) findViewById(R.id.encoder_spinner);
    decoderSelectionButton = (Button) findViewById(R.id.decoder_selection_button);
    decodersTextView = (TextView) findViewById(R.id.decoders_text_view);
    sessionButton = (ToggleButton) findViewById(R.id.session_button);
    switchLayout = (RelativeLayout) findViewById(R.id.switch_layout);
    sendSwitch = (Switch) findViewById(R.id.start_send_switch);
    playoutSwitch = (Switch) findViewById(R.id.start_playout_switch);

    setUpIPAddressEditTexts();
    setUpEncoderSpinner();
    setUpDecoderSelectionButton();
    setUpSessionButton();
    setUpSendAndPlayoutSwitch();
  }

  private void setUpEncoderSpinner() {
    ArrayAdapter<String> encoderAdapter =
        new ArrayAdapter<String>(this, android.R.layout.simple_spinner_item, supportedEncoders);
    encoderAdapter.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);
    encoderSpinner.setAdapter(encoderAdapter);
    encoderSpinner.setOnItemSelectedListener(new AdapterView.OnItemSelectedListener() {
      @Override
      public void onItemSelected(AdapterView<?> parent, View view, int position, long id) {
        String encoder = (String) parent.getSelectedItem();
        voipClient.setEncoder(encoder);
      }
      @Override
      public void onNothingSelected(AdapterView<?> parent) {}
    });
  }

  private List<String> getSelectedDecoders() {
    List<String> decoders = new ArrayList<>();
    for (int i = 0; i < supportedDecoders.size(); i++) {
      if (selectedDecoders.contains(i)) {
        decoders.add(supportedDecoders.get(i));
      }
    }
    return decoders;
  }

  private void setUpDecoderSelectionButton() {
    decoderSelectionButton.setOnClickListener((view) -> {
      AlertDialog.Builder dialogBuilder = new AlertDialog.Builder(this);
      dialogBuilder.setTitle(R.string.dialog_title);

      // Populate multi choice items with supported decoders
      String[] supportedDecodersArray = supportedDecoders.toArray(new String[0]);
      dialogBuilder.setMultiChoiceItems(
          supportedDecodersArray, isDecoderSelected, (dialog, position, isChecked) -> {
            if (isChecked) {
              selectedDecoders.add(position);
            } else if (!isChecked) {
              selectedDecoders.remove(position);
            }
          });

      // "Ok" button
      dialogBuilder.setPositiveButton(R.string.ok_label, (dialog, position) -> {
        List<String> decoders = getSelectedDecoders();
        String result = decoders.stream().collect(Collectors.joining(", "));
        if (result.isEmpty()) {
          decodersTextView.setText(R.string.decoders_text_view_default);
        } else {
          decodersTextView.setText(result);
        }
        voipClient.setDecoders(decoders);
      });

      // "Dismiss" button
      dialogBuilder.setNegativeButton(
          R.string.dismiss_label, (dialog, position) -> { dialog.dismiss(); });

      // "Clear All" button
      dialogBuilder.setNeutralButton(R.string.clear_all_label, (dialog, position) -> {
        Arrays.fill(isDecoderSelected, false);
        selectedDecoders.clear();
        decodersTextView.setText(R.string.decoders_text_view_default);
      });

      AlertDialog dialog = dialogBuilder.create();
      dialog.show();
    });
  }

  private boolean startSession() {
    // order matters here, addresses have to be set before starting session before setting codec
    voipClient.setLocalAddress(localIPAddressTextView.getText().toString(),
        Integer.parseInt(localPortNumberEditText.getText().toString()));
    voipClient.setRemoteAddress(remoteIPAddressEditText.getText().toString(),
        Integer.parseInt(remotePortNumberEditText.getText().toString()));
    boolean didStart = voipClient.startSession();
    voipClient.setEncoder((String) encoderSpinner.getSelectedItem());
    voipClient.setDecoders(getSelectedDecoders());
    return didStart;
  }

  private void setUpSessionButton() {
    sessionButton.setOnCheckedChangeListener((button, isChecked) -> {
      // only proceed if client has been initialized
      if (!isClientInitialized) {
        isClientInitialized = voipClient.initialize();
        if (!isClientInitialized) {
          showToast("Error initilizing");
          button.setChecked(false);
          return;
        }
      }
      if (isChecked) {
        if (startSession()) {
          showToast("Session started");
        } else {
          showToast("Failed to start session");
          button.setChecked(false);
          return;
        }
        switchLayout.setVisibility(View.VISIBLE);
        scrollView.post(() -> { scrollView.fullScroll(ScrollView.FOCUS_DOWN); });
      } else {
        if (voipClient.stopSession()) {
          showToast("Session stopped");
        } else {
          showToast("Failed to stop session");
          button.setChecked(true);
          return;
        }
        // set listners to null so the checked state can be changed programmatically
        sendSwitch.setOnCheckedChangeListener(null);
        playoutSwitch.setOnCheckedChangeListener(null);
        sendSwitch.setChecked(false);
        playoutSwitch.setChecked(false);
        // redo the switch listener setup
        setUpSendAndPlayoutSwitch();
        switchLayout.setVisibility(View.GONE);
      }
    });
  }

  private void setUpSendAndPlayoutSwitch() {
    sendSwitch.setOnCheckedChangeListener((button, isChecked) -> {
      if (isChecked) {
        // Ask for permission on RECORD_AUDIO if not granted
        // If user declines, startSend() will fail and user will be notified by the toast
        if (ContextCompat.checkSelfPermission(this, permission.RECORD_AUDIO)
            != PackageManager.PERMISSION_GRANTED) {
          String[] sList = {permission.RECORD_AUDIO};
          ActivityCompat.requestPermissions(this, sList, 1);
        }

        if (voipClient.startSend()) {
          showToast("Started sending");
        } else {
          showToast("Error initializing microphone");
          button.setChecked(false);
        }
      } else {
        if (voipClient.stopSend()) {
          showToast("Stopped sending");
        } else {
          showToast("Microphone termination failed");
          button.setChecked(true);
        }
      }
    });

    playoutSwitch.setOnCheckedChangeListener((button, isChecked) -> {
      if (isChecked) {
        if (voipClient.startPlayout()) {
          showToast("Started playout");
        } else {
          showToast("Error initializing speaker");
          button.setChecked(false);
        }
      } else {
        if (voipClient.stopPlayout()) {
          showToast("Stopped playout");
        } else {
          showToast("Speaker termination failed");
          button.setChecked(true);
        }
      }
    });
  }

  private void setUpIPAddressEditTexts() {
    String localIPAddress = voipClient.getLocalIPAddress();
    if (localIPAddress.isEmpty()) {
      showToast("Please check your network configuration");
    } else {
      localIPAddressTextView.setText(localIPAddress);
      // By default remote IP address is the same as local IP address
      remoteIPAddressEditText.setText(localIPAddress);
    }
  }

  private void showToast(String message) {
    toast.cancel();
    toast = Toast.makeText(this, message, Toast.LENGTH_SHORT);
    toast.setGravity(Gravity.TOP, 0, 200);
    toast.show();
  }

  @Override
  protected void onDestroy() {
    voipClient.close();
    voipClient = null;

    super.onDestroy();
  }
}
