#pragma once

#include "ca/domain/destination_catalog.hpp"
#include "ca/station_request_controller.hpp"
#include "ca/ui.hpp"

namespace ca {

class BasicStationUserInterface final : public StationUserInterface {
public:
  BasicStationUserInterface(DestinationCatalog& destinations,
                            StationRequestController& requests);

  void onIntent(const UiIntent& intent) override;
  UiViewModel view() const override;

private:
  DestinationCatalog& destinations_;
  StationRequestController& requests_;
  uint16_t selectedIndex_;
  DeviceId selectedDestinationId_;
  char status_[17];

  void moveSelection(int8_t direction);
  void setStatus(const char* message);
  static void copyLine(char output[17], const char* input);
};

class BasicCartUserInterface final : public CartUserInterface {
public:
  BasicCartUserInterface();

  void onIntent(const UiIntent& intent, const CartUiContext& context) override;
  void update(const CartUiContext& context) override;
  UiViewModel view() const override;

private:
  UiViewModel view_;
  void setLine(uint8_t row, const char* message);
};

} // namespace ca
