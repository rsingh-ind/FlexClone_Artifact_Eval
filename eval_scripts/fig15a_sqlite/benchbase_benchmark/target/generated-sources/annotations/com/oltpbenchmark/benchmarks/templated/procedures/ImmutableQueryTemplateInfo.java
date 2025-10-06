package com.oltpbenchmark.benchmarks.templated.procedures;

import com.oltpbenchmark.api.SQLStmt;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.Objects;
import org.immutables.value.Generated;

/**
 * Immutable implementation of {@link GenericQuery.QueryTemplateInfo}.
 * <p>
 * Use the builder to create immutable instances:
 * {@code ImmutableQueryTemplateInfo.builder()}.
 */
@Generated(from = "GenericQuery.QueryTemplateInfo", generator = "Immutables")
@SuppressWarnings({"all"})
@javax.annotation.processing.Generated("org.immutables.processor.ProxyProcessor")
public final class ImmutableQueryTemplateInfo
    implements GenericQuery.QueryTemplateInfo {
  private final SQLStmt query;
  private final String[] paramsTypes;
  private final String[] paramsValues;

  private ImmutableQueryTemplateInfo(
      SQLStmt query,
      String[] paramsTypes,
      String[] paramsValues) {
    this.query = query;
    this.paramsTypes = paramsTypes;
    this.paramsValues = paramsValues;
  }

  /**
   *Query string for this template. 
   */
  @Override
  public SQLStmt getQuery() {
    return query;
  }

  /**
   *Query parameter types. 
   */
  @Override
  public String[] getParamsTypes() {
    return paramsTypes.clone();
  }

  /**
   *Potential query parameter values. 
   */
  @Override
  public String[] getParamsValues() {
    return paramsValues.clone();
  }

  /**
   * Copy the current immutable object by setting a value for the {@link GenericQuery.QueryTemplateInfo#getQuery() query} attribute.
   * A shallow reference equality check is used to prevent copying of the same value by returning {@code this}.
   * @param value A new value for query
   * @return A modified copy of the {@code this} object
   */
  public final ImmutableQueryTemplateInfo withQuery(SQLStmt value) {
    if (this.query == value) return this;
    SQLStmt newValue = Objects.requireNonNull(value, "query");
    return new ImmutableQueryTemplateInfo(newValue, this.paramsTypes, this.paramsValues);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link GenericQuery.QueryTemplateInfo#getParamsTypes() paramsTypes}.
   * The array is cloned before being saved as attribute values.
   * @param elements The non-null elements for paramsTypes
   * @return A modified copy of {@code this} object
   */
  public final ImmutableQueryTemplateInfo withParamsTypes(String... elements) {
    String[] newValue = elements.clone();
    return new ImmutableQueryTemplateInfo(this.query, newValue, this.paramsValues);
  }

  /**
   * Copy the current immutable object with elements that replace the content of {@link GenericQuery.QueryTemplateInfo#getParamsValues() paramsValues}.
   * The array is cloned before being saved as attribute values.
   * @param elements The non-null elements for paramsValues
   * @return A modified copy of {@code this} object
   */
  public final ImmutableQueryTemplateInfo withParamsValues(String... elements) {
    String[] newValue = elements.clone();
    return new ImmutableQueryTemplateInfo(this.query, this.paramsTypes, newValue);
  }

  /**
   * This instance is equal to all instances of {@code ImmutableQueryTemplateInfo} that have equal attribute values.
   * @return {@code true} if {@code this} is equal to {@code another} instance
   */
  @Override
  public boolean equals(Object another) {
    if (this == another) return true;
    return another instanceof ImmutableQueryTemplateInfo
        && equalTo(0, (ImmutableQueryTemplateInfo) another);
  }

  private boolean equalTo(int synthetic, ImmutableQueryTemplateInfo another) {
    return query.equals(another.query)
        && Arrays.equals(paramsTypes, another.paramsTypes)
        && Arrays.equals(paramsValues, another.paramsValues);
  }

  /**
   * Computes a hash code from attributes: {@code query}, {@code paramsTypes}, {@code paramsValues}.
   * @return hashCode value
   */
  @Override
  public int hashCode() {
    int h = 5381;
    h += (h << 5) + query.hashCode();
    h += (h << 5) + Arrays.hashCode(paramsTypes);
    h += (h << 5) + Arrays.hashCode(paramsValues);
    return h;
  }

  /**
   * Prints the immutable value {@code QueryTemplateInfo} with attribute values.
   * @return A string representation of the value
   */
  @Override
  public String toString() {
    return "QueryTemplateInfo{"
        + "query=" + query
        + ", paramsTypes=" + Arrays.toString(paramsTypes)
        + ", paramsValues=" + Arrays.toString(paramsValues)
        + "}";
  }

  /**
   * Creates an immutable copy of a {@link GenericQuery.QueryTemplateInfo} value.
   * Uses accessors to get values to initialize the new immutable instance.
   * If an instance is already immutable, it is returned as is.
   * @param instance The instance to copy
   * @return A copied immutable QueryTemplateInfo instance
   */
  public static ImmutableQueryTemplateInfo copyOf(GenericQuery.QueryTemplateInfo instance) {
    if (instance instanceof ImmutableQueryTemplateInfo) {
      return (ImmutableQueryTemplateInfo) instance;
    }
    return ImmutableQueryTemplateInfo.builder()
        .from(instance)
        .build();
  }

  /**
   * Creates a builder for {@link ImmutableQueryTemplateInfo ImmutableQueryTemplateInfo}.
   * <pre>
   * ImmutableQueryTemplateInfo.builder()
   *    .query(com.oltpbenchmark.api.SQLStmt) // required {@link GenericQuery.QueryTemplateInfo#getQuery() query}
   *    .paramsTypes(String) // required {@link GenericQuery.QueryTemplateInfo#getParamsTypes() paramsTypes}
   *    .paramsValues(String) // required {@link GenericQuery.QueryTemplateInfo#getParamsValues() paramsValues}
   *    .build();
   * </pre>
   * @return A new ImmutableQueryTemplateInfo builder
   */
  public static ImmutableQueryTemplateInfo.Builder builder() {
    return new ImmutableQueryTemplateInfo.Builder();
  }

  /**
   * Builds instances of type {@link ImmutableQueryTemplateInfo ImmutableQueryTemplateInfo}.
   * Initialize attributes and then invoke the {@link #build()} method to create an
   * immutable instance.
   * <p><em>{@code Builder} is not thread-safe and generally should not be stored in a field or collection,
   * but instead used immediately to create instances.</em>
   */
  @Generated(from = "GenericQuery.QueryTemplateInfo", generator = "Immutables")
  public static final class Builder {
    private static final long INIT_BIT_QUERY = 0x1L;
    private static final long INIT_BIT_PARAMS_TYPES = 0x2L;
    private static final long INIT_BIT_PARAMS_VALUES = 0x4L;
    private long initBits = 0x7L;

    private SQLStmt query;
    private String[] paramsTypes;
    private String[] paramsValues;

    private Builder() {
    }

    /**
     * Fill a builder with attribute values from the provided {@code QueryTemplateInfo} instance.
     * Regular attribute values will be replaced with those from the given instance.
     * Absent optional values will not replace present values.
     * @param instance The instance from which to copy values
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder from(GenericQuery.QueryTemplateInfo instance) {
      Objects.requireNonNull(instance, "instance");
      this.query(instance.getQuery());
      this.paramsTypes(instance.getParamsTypes());
      this.paramsValues(instance.getParamsValues());
      return this;
    }

    /**
     * Initializes the value for the {@link GenericQuery.QueryTemplateInfo#getQuery() query} attribute.
     * @param query The value for query 
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder query(SQLStmt query) {
      this.query = Objects.requireNonNull(query, "query");
      initBits &= ~INIT_BIT_QUERY;
      return this;
    }

    /**
     * Initializes the value for the {@link GenericQuery.QueryTemplateInfo#getParamsTypes() paramsTypes} attribute.
     * @param paramsTypes The elements for paramsTypes
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder paramsTypes(String... paramsTypes) {
      this.paramsTypes = paramsTypes.clone();
      initBits &= ~INIT_BIT_PARAMS_TYPES;
      return this;
    }

    /**
     * Initializes the value for the {@link GenericQuery.QueryTemplateInfo#getParamsValues() paramsValues} attribute.
     * @param paramsValues The elements for paramsValues
     * @return {@code this} builder for use in a chained invocation
     */
    public final Builder paramsValues(String... paramsValues) {
      this.paramsValues = paramsValues.clone();
      initBits &= ~INIT_BIT_PARAMS_VALUES;
      return this;
    }

    /**
     * Builds a new {@link ImmutableQueryTemplateInfo ImmutableQueryTemplateInfo}.
     * @return An immutable instance of QueryTemplateInfo
     * @throws java.lang.IllegalStateException if any required attributes are missing
     */
    public ImmutableQueryTemplateInfo build() {
      if (initBits != 0) {
        throw new IllegalStateException(formatRequiredAttributesMessage());
      }
      return new ImmutableQueryTemplateInfo(query, paramsTypes, paramsValues);
    }

    private String formatRequiredAttributesMessage() {
      List<String> attributes = new ArrayList<>();
      if ((initBits & INIT_BIT_QUERY) != 0) attributes.add("query");
      if ((initBits & INIT_BIT_PARAMS_TYPES) != 0) attributes.add("paramsTypes");
      if ((initBits & INIT_BIT_PARAMS_VALUES) != 0) attributes.add("paramsValues");
      return "Cannot build QueryTemplateInfo, some of required attributes are not set " + attributes;
    }
  }
}
