import Foundation

import PlaygroundSupport


struct Solar: Codable

{

    var panel_temp: Double

    var date_time: String

    var station: String

    var uptime: Int

    var soc: Int

    var battery_voltage: Double

    var controller_temp: Int

    var panel_voltage: Double

    var load_voltage: Double

    var load_current: Double

    var load_power: Int

    var panel_current: Int

    var charge_power: Int

    var min_voltage_of_day: Double

    var max_voltage_of_day: Double

    var max_charge_current_of_day: Double

    var max_discharge_current_of_day: Double

    var max_charge_power_of_day: Int

    var max_discharge_power_of_day: Int

    var charge_amphrs_of_day: Int

    var discharge_amphrs_of_day: Int

    var power_generated_current_day: Int

    var power_consumed_current_day: Int

    var battery_type: String

    var charge_state: String

    

    enum CodingKeys: String, CodingKey

    {

        // had to do this, two of the variables were not compliant with swift naming convention having a dash in the name.  This just gives an alias

        case charge_amphrs_of_day = "charge_amp-hrs_of_day"

        case discharge_amphrs_of_day = "discharge_amp-hrs_of_day"

        case charge_state

        case battery_type

        case power_consumed_current_day

        case power_generated_current_day

        case max_discharge_power_of_day

        case max_charge_power_of_day

        case max_discharge_current_of_day

        case max_charge_current_of_day

        case max_voltage_of_day

        case min_voltage_of_day

        case charge_power

        case panel_current

        case load_power

        case load_current

        case load_voltage

        case panel_voltage

        case controller_temp

        case battery_voltage

        case soc

        case uptime

        case station

        case date_time

        case panel_temp

    }

}

//get data from internal web as string//////////////////////////////////////

if let url = URL(string: "http://192.168.1.39:8585") {

    do {

        // read in contents sent by nodemcu over http

        let contents = try String(contentsOf: url)

        // print(contents)

        

        // save contents as json data

        let jsonData = contents.data(using: .utf8)

        

        // Decode the json data into "solar" struct

        let decoder = JSONDecoder()

        let solar = try! decoder.decode(Solar.self, from: jsonData!)

        

        // test to see that struct populates

        // somtimes get an error from incomplete read over network

        // retry

        // print(solar.battery_voltage)

        

    } catch {

        // contents could not be loaded

        print("Problem loading contents")

    }

} else {

    // the URL was bad!

    print("Bad URL")

}

//////////////////////////////////////////////////////////////////
