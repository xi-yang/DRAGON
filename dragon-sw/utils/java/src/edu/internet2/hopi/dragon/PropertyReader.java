package edu.internet2.hopi.dragon;

import java.io.FileInputStream;
import java.io.IOException;
import java.util.Properties;

/**
 * Reads properties from a property file. Properties take form of <i>propertyName=value</i>.
 * @author Andrew Lake (alake@internet2.edu)
 *
 */
public class PropertyReader {
	Properties props;
	
	/**
	 * Loads properties from given property file
	 * @param filename path to property file
	 * @throws IOException
	 */
	public PropertyReader(String filename) throws IOException{
		/* Initialize */
		props = new Properties();
		
		/* load properties from file */
		FileInputStream in = new FileInputStream(filename);
        props.load(in);
        in.close();
	}
	
	/**
	 * Returns the value of a given property as a String
	 * @param propertyName name of property to retrieve
	 * @return value of given property as a String
	 */
	public String getProperty(String propertyName){
		return props.getProperty(propertyName);
	}
}
